#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
// A normal C function that is executed as a thread 
// when its name is specified in pthread_create()

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//An array of kernel matrices to be used for image convolution.  
//The indexes of these match the enumeration from the header file. ie. algorithms[BLUR] returns the kernel corresponding to a box blur.
Matrix algorithms[]={
    {{0,-1,0},{-1,4,-1},{0,-1,0}},
    {{0,-1,0},{-1,5,-1},{0,-1,0}},
    {{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0}},
    {{1.0/16,1.0/8,1.0/16},{1.0/8,1.0/4,1.0/8},{1.0/16,1.0/8,1.0/16}},
    {{-2,-1,0},{-1,1,1},{0,1,2}},
    {{0,0,0},{0,1,0},{0,0,0}}
};

uint8_t getPixelValue(Image* srcImage,int x,int y,int bit,Matrix algorithm){
    int px,mx,py,my,i,span;
    span=srcImage->width*srcImage->bpp;
    // for the edge pixes, just reuse the edge pixel
    px=x+1; py=y+1; mx=x-1; my=y-1;
    if (mx<0) mx=0;
    if (my<0) my=0;
    if (px>=srcImage->width) px=srcImage->width-1;
    if (py>=srcImage->height) py=srcImage->height-1;
    uint8_t result=
        algorithm[0][0]*srcImage->data[Index(mx,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][1]*srcImage->data[Index(x,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][2]*srcImage->data[Index(px,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][0]*srcImage->data[Index(mx,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][1]*srcImage->data[Index(x,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][2]*srcImage->data[Index(px,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][0]*srcImage->data[Index(mx,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][1]*srcImage->data[Index(x,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][2]*srcImage->data[Index(px,py,srcImage->width,bit,srcImage->bpp)];
    return result;
}

//Usage: Prints usage information for the program
//Returns: -1
int Usage(){
    printf("Usage: image <filename> <type>\n\twhere type is one of (edge,sharpen,blur,gauss,emboss,identity)\n");
    return -1;
}

//GetKernelType: Converts the string name of a convolution into a value from the KernelTypes enumeration
//Parameters: type: A string representation of the type
//Returns: an appropriate entry from the KernelTypes enumeration, defaults to IDENTITY, which does nothing but copy the image.
enum KernelTypes GetKernelType(char* type){
    if (!strcmp(type,"edge")) return EDGE;
    else if (!strcmp(type,"sharpen")) return SHARPEN;
    else if (!strcmp(type,"blur")) return BLUR;
    else if (!strcmp(type,"gauss")) return GAUSE_BLUR;
    else if (!strcmp(type,"emboss")) return EMBOSS;
    else return IDENTITY;
}


struct thd_arg{
    int startrow;
    int endrow;
    Image *srcimage;
    Image *destimage;
    int type;
} ;

void *myThreadFun(void *arg)
{
    struct thd_arg *a = (struct thd_arg*)arg;
    int row,pix,bit,span;
    Image *srcImage = (*a).srcimage;
    Image *destImage = (*a).destimage;
    span=srcImage->bpp*srcImage->bpp;
    for (row=(*a).startrow;row<(*a).endrow;row++){
        for (pix=0;pix<srcImage->width;pix++){
            for (bit=0;bit<srcImage->bpp;bit++){
                destImage->data[Index(pix,row,srcImage->width,bit,srcImage->bpp)]=getPixelValue(srcImage,pix,row,bit,algorithms[(*a).type]);
            }
        }
    }

    return NULL;
}

int main(int argc,char** argv)
{   
    long t1,t2;
    t1=time(NULL);

    stbi_set_flip_vertically_on_load(0); 
    if (argc!=3) return Usage();
    char* fileName=argv[1];
    if (!strcmp(argv[1],"pic4.jpg")&&!strcmp(argv[2],"gauss")){
        printf("You have applied a gaussian filter to Gauss which has caused a tear in the time-space continum.\n");
    }
    enum KernelTypes type=GetKernelType(argv[2]);

    Image srcImage,destImage,bwImage;   
    srcImage.data=stbi_load(fileName,&srcImage.width,&srcImage.height,&srcImage.bpp,0);
    if (!srcImage.data){
        printf("Error loading file %s.\n",fileName);
        return -1;
    }
    destImage.bpp=srcImage.bpp;
    destImage.height=srcImage.height;
    destImage.width=srcImage.width;
    destImage.data=malloc(sizeof(uint8_t)*destImage.width*destImage.bpp*destImage.height);

    int thread_count = 5;
    int i;
    pthread_t thread_id [thread_count];
    struct thd_arg thread_arg[thread_count];
    int perjob = srcImage.height/thread_count;

    for (i = 0; i < thread_count;i++){
        thread_arg[i].srcimage = &srcImage;
        thread_arg[i].destimage = &destImage;
        thread_arg[i].type = type;
        thread_arg[i].startrow = i*perjob;
        if(i == thread_count-1){
            thread_arg[i].endrow = srcImage.height;
        }else{
            thread_arg[i].endrow = (i+1)*perjob-1;
        }
    }
    //printf("total rows:%d \n", srcImage.height);
    for (i = 0; i < thread_count;i++){
       //printf("number of thread: %d start:%d end:%d\n",i,thread_arg[i].startrow,thread_arg[i].endrow);
        pthread_create(&thread_id[i], NULL, myThreadFun, &thread_arg[i]);
    }
    for (i = 0; i < thread_count;i++){
        pthread_join(thread_id[i], NULL);
    }
    //SSSSprintf("After Thread\n");

    stbi_write_png("output.png",destImage.width,destImage.height,destImage.bpp,destImage.data,destImage.bpp*destImage.width);
    stbi_image_free(srcImage.data);
    free(destImage.data);
    t2=time(NULL);
    printf("Took %ld seconds\n",t2-t1);
    return 0;
}
