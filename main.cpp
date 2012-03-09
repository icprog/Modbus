#include <iostream>
#include <vector>
#include <exception>
#include "modbus.h"
#include "modbus-private.h"
#include "NetServer.h"
#include "config.h"

#ifndef _MSC_VER
#include <signal.h>
#endif

#define BOOST_DATE_TIME_NO_LIB
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>




using namespace boost::interprocess;
using namespace std;
using boost::asio::ip::tcp;


//typedef boost::shared_ptr<tcp::socket> socket_ptr;




bool bQuit = false ;
bool showCommData =false ;
RAW_COMM_DATA recv_raw ;

Config config ;
vector < boost::shared_ptr<boost::thread> > threads ;

int getCommPortByAddr(uint8_t addr)
{
        for(size_t n = 0;n<config.busLines.size();n++)
        {
            for(size_t m = 0 ;m<config.busLines[n].modules.size() ;m++)
            {
                if(addr == config.busLines[n].modules[m].addr)
                    return n+1 ;
            }
        }
}

extern "C" {

void busMonitorSendData(uint8_t *data,uint8_t dataLen)
{
    int com = getCommPortByAddr(data[0]) ;
    RAW_COMM_DATA raw ;
    raw.length = dataLen ;
    raw.isRecv = 0 ;
    memcpy(raw.data,data,dataLen) ;
    {
        boost::mutex::scoped_lock lock(commData_mutex) ;
        rawCommDatas[com].push_back(raw);
    }
    if(showCommData )
    {
        for(uint8_t i = 0 ; i< dataLen; i++)
        {
            printf("%.2X ", data[i]);
        }
        printf("\n");
    }

    //try{

    //	shared_memory_object sharedMem(open_or_create,"Test",read_write);
    //	sharedMem.truncate(256);
    //	mapped_region mmap(sharedMem,read_write);
    //
    //	memcpy(mmap.get_address(),&n,4);
    //	memcpy((char*)(mmap.get_address())+4,&m,4);
    //	memcpy((char*)(mmap.get_address())+8,&addr,sizeof(uint16_t));
    //	memcpy((char*)(mmap.get_address())+8+sizeof(uint16_t),&nb,sizeof(uint16_t));

    //	shared_memory_object::remove("Test");
    //}
    //catch(interprocess_exception &e)
    //{
    //	/*shared_memory_object::remove("Test");*/
    //	cout<<e.what()<<endl;
    //}
}

void busMonitorRecvData(uint8_t * data, uint8_t dataLen,int addNewLine )
{
    memcpy(recv_raw.data+recv_raw.length,data,dataLen);
    recv_raw.length += dataLen ;
    if(addNewLine)
    {
        recv_raw.isRecv = 1 ;
        int com = getCommPortByAddr(recv_raw.data[0]) ;
        {
            boost::mutex::scoped_lock lock(commData_mutex) ;
            rawCommDatas[com].push_back(recv_raw);
        }
        recv_raw.length = 0;
    }
//    int com = getCommPortByAddr(data[0]) ;
  //  raw.add = addNewLine ;
    //raw.length = dataLen ;
    //memcpy(raw.data,data,dataLen) ;



    if(showCommData)
    {
        for(uint8_t i = 0 ; i< dataLen; i++)
        {
            printf("%.2X ",data[i]);
        }
        if(addNewLine)
            printf("\n");
    }
}

}
#if defined(_WIN32)
BOOL WINAPI controlHandler(DWORD t)
{
    if(t==CTRL_BREAK_EVENT||t==CTRL_C_EVENT)
        bQuit = true ;
    for(size_t i = 0 ;i < threads.size();i++)
        threads[i]->join();
    exit(1);
    return true ;
}

#else
void controlHandler(int)
{
    bQuit = true ;
    for(size_t i = 0 ;i < threads.size();i++)
        threads[i]->join();
    exit(1);
}

#endif


void workerThread(void* p)
{
    Bus *bus = (Bus*)p ;
    modbus_t *modbus;
    //modbus = modbus_new_rtu("/dev/ttyS0",9600,'N',8,1);
    modbus = modbus_new_rtu(bus->sPort,bus->baud,bus->parity,bus->databits,bus->stopbits);
    if( modbus_connect( modbus ) == -1 )
    {
        cout<<"connect "<<bus->sPort<<" error"<<endl ;
        return  ;
    }
    uint8_t dest[1024];
    uint16_t * dest16 = (uint16_t *) dest;
    while(!bQuit)
    {
        int ret ;
        memset( dest, 0, 1024 );

        for(size_t i = 0 ;i < bus->modules.size();i++)
        {
            for(size_t j = 0 ; j<bus->modules[i].reqs.size();j++)
            {
                modbus_set_slave( modbus, bus->modules[i].addr );
                switch(bus->modules[i].reqs[j].reqType)
                {
                case _FC_READ_COILS:
                    ret = modbus_read_bits( modbus, bus->modules[i].reqs[j].reg, bus->modules[i].reqs[j].num, dest );
                    break;
                case _FC_READ_DISCRETE_INPUTS:
                    ret = modbus_read_input_bits( modbus, bus->modules[i].reqs[j].reg, bus->modules[i].reqs[j].num, dest );
                    break;
                case _FC_READ_HOLDING_REGISTERS:
                    ret = modbus_read_registers( modbus, bus->modules[i].reqs[j].reg, bus->modules[i].reqs[j].num, dest16 );
                    break;
                case _FC_READ_INPUT_REGISTERS:
                    ret = modbus_read_input_registers( modbus, bus->modules[i].reqs[j].reg, bus->modules[i].reqs[j].num, dest16 );
                    break;
                }


                boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            }
        }
    }
    modbus_close(modbus);
    modbus_free(modbus);
}
void breakHandle()
{
#if defined(_WIN32)
    SetConsoleCtrlHandler(controlHandler, true);
#else
    struct sigaction act;
    memset (&act, '\0', sizeof(act));
    act.sa_handler = &controlHandler;
    if (sigaction(SIGINT, &act, NULL) < 0) {
            cout<<"set break handler sig error"<<endl;
    }
#endif
}


//comm data monitor server thread
void monitorThread()
{
    boost::asio::io_service io_service;

    try
    {
        server s(io_service, LISTEN_PORT);
        while(!bQuit)
        {
            io_service.poll();
            boost::this_thread::sleep(boost::posix_time::milliseconds(1)) ;
        }
        io_service.stop();
    }catch(std::exception &e)
    {
        cout<<"Exception:"<<e.what()<<endl ;
    }
}

int main()
{
    breakHandle();
    try{
        config.load();
    }catch(exception &e)
    {
        cout<<e.what()<<endl ;
        return 1 ;
    }

    cout<<config.bus_number<<endl ;



    //启动通道数据监视监听线程，默认监听端口10010
    boost::shared_ptr<boost::thread> thread(new boost::thread (monitorThread));
    threads.push_back(thread);

    //根据配置信息分别启动各个串行口的数据采集线程
    for(int i = 0 ;i < config.bus_number ;i++)
    {
        boost::circular_buffer<RAW_COMM_DATA> datas(16) ;
        rawCommDatas[i+1] = datas ;
        boost::shared_ptr<boost::thread> thread(new boost::thread (boost::bind(workerThread,&config.busLines[0])));
        threads.push_back(thread);
    }

    //循环处理用户终端输入，q--退出；d-- 显示报文； p-- 停止显示报文
    //TODO：终端显示报文未同步，多个串口采集时显示报文可能乱序
    char s = 0;
    while(s!='q')
    {
        cin>>s  ;
        if(s =='p')
            showCommData = false ;
        else if(s=='d')
            showCommData = true ;
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }

    bQuit = true ;
    //等待所有工作线程退出
    for(size_t i = 0 ;i < threads.size();i++)
        threads[i]->join();
    return 0;
}

