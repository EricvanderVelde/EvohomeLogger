#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>		//Used for UART
#include <fcntl.h>		//Used for UART
#include <termios.h>		//Used for UART	
#include <pthread.h>
#include <sys/uio.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <cstddef>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <fstream>

#include "influxdb.hpp"

const std::string currentTime() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%X", &tstruct);

    return buf;
}

class HR82
{
    public:
        HR82() {_zone=-1;_setPoint=0;_temp=0;}

        void setName(std::string name) {_name=name;}
        std::string Name(void) {return _name;}

        void setSetPoint(float setPoint) {_setPoint=setPoint;}
        float SetPoint(void) {return _setPoint;}

        void setTemp(float temp) {_temp=temp;}
        float Temp(void) {return _temp;}

        void setZone(int zone) {_zone=zone;}
        int Zone(void) {return _zone;}

        inline bool operator == (const HR82 &b) const { return b._name==_name; }
        void print(void);

    private:
        std::string  _name;
        float        _setPoint;
        float        _temp;
        int          _zone; 
};

void HR82::print(void)
{
    std::cout << "[";
    std::cout << _name;
    std::cout << " S:" << std::setfill (' ') << std::fixed << std::setw (5) << std::setprecision(2) << _setPoint;
    std::cout << " T:" << std::setfill (' ') << std::fixed << std::setw (5) << std::setprecision(2) <<_temp;
    std::cout << "]";
}

class Room
{
    public:
        void setName(std::string name) {_name=name;}
        std::string getName(void) {return _name;}

        void setSetPoint(float setPoint) {_setPoint=setPoint;}
        double getSetPoint(void) {return _setPoint;}

        void setTemp(float temp) {_temp=temp;}
        double getTemp(void) {return _temp;}

        void setZone(int zone) {_zone=zone;}
        void print(void);

    private:
        std::string  _name;
        float        _setPoint;
        float        _temp; 
        int          _zone;
};

std::map <std::string, HR82> HR82s;

void Room::print(void)
{
    if  (_temp == 0 && _setPoint == 0)
    {
        return;
    }

    std::cout << currentTime(); 
    std::cout << " Room [" << std::setw (10) << _name << "]";
    std::cout << " Set [" << std::setfill (' ') << std::fixed << std::setw (5) << std::setprecision(2) << _setPoint << "]";
    std::cout << " T [" << std::setfill (' ') << std::fixed << std::setw (5) << std::setprecision(2) << _temp << "]";

    for (auto &i: HR82s)
    {
        if (i.second.Zone() == _zone)
        {
            i.second.print();
        }
    }
      
    std::cout <<  std::endl;
}

std::map <int, Room> Rooms;

int uart0_filestream = -1;
FILE *in=0;

struct split
{
  enum empties_t { empties_ok, no_empties };
};

template <typename Container>
Container& split(
    Container&                            result,
    const typename Container::value_type& s,
    const typename Container::value_type& delimiters,
    split::empties_t                      empties = split::empties_ok )
{
    result.clear();
    size_t current;
    size_t next = -1;

    do
    {
        if (empties == split::no_empties)
        {
            next = s.find_first_not_of( delimiters, next + 1 );

            if (next == Container::value_type::npos)
            {
                break;
            }
            next -= 1;
        }

        current = next + 1;
        next = s.find_first_of( delimiters, current );
        result.push_back(s.substr( current, next - current ));
    }
    while (next != Container::value_type::npos);

    return result;
}

void uart_setup()
{
    uart0_filestream = open("/dev/ttyUSB0", O_RDWR);	

    if (uart0_filestream == -1)
    {
        printf("Error - Unable to open UART (%s).\n", strerror(errno));
    }

    in = fopen("/dev/ttyUSB0", "r");
}

int uart_read(std::string &message)
{
    char buffer[256];
    memset((void*)buffer, 0 , 255);

    if  (fgets(buffer, 255, in) != 0)
    {
        for     (int i = 0; i <= strlen(buffer)-2; i++)
        {
            if  (!iscntrl(buffer[i])) 
            {
                message += buffer[i];
            }
        }
    }
}

void uart_close()
{
	close(uart0_filestream);
}

void print(std::vector <std::string> & v)
{
    for (size_t n = 0; n < v.size(); n++)
    { 
        std::cout << "\"" << v[ n ] << "\" ";
    }
    std::cout << std::endl;
}

void reportValues(void)
{
    while(1)
    {
        sleep(60);

        for (auto &i:HR82s)
        {
            if  (i.second.Zone() == -1)
            {
                i.second.print();
                std::cout << std::endl;
            }
        }

        influxdb_cpp::server_info si("127.0.0.1", 8086, "evohome");

        for (auto &i:Rooms)
        {
            i.second.print();

            if (i.second.getName().length() != 0)
            {
                int ret = influxdb_cpp::builder()
                    .meas("Room_Temperature")
                    .field("name", i.second.getName())
                    .field("setpoint_temperature", i.second.getSetPoint(), 5)
                    .field("room_temperature", i.second.getTemp(), 5)
                    .post_http(si);
            }
        }
    }
}

int main(void)
{
    uart_setup();

    pthread_t reportThread;
    pthread_create(&reportThread, NULL, (void* (*)(void*))&reportValues, (void*)NULL);

    for (int i=0; i<10; i++)
    {
        Rooms.insert(std::pair<int, Room>(i, Room()));
        Rooms[i].setZone(i);
    }

    while(1)
    {
        std::string msg;
        uart_read(msg);

        //cout << currentTime() << " ? " << "[" << msg << "]" << std::endl;

        bool handled = false;

        std::vector <std::string> fields;
        split( fields, msg, " ", split::no_empties);

//045  I --- 01:048853 --:------ 01:048853 2309 021 0001F40101F40201F40301F40401F40501F40601F4
        if      (fields[6].compare("2309") == 0 && fields[8].length() > 10)
        {
            //cout << "Setpoint Temparature" << std::endl;

            for (int i=0; i<atoi(fields[7].c_str())/3; i++)
            {
                std::string temp = fields[8].substr(i*6+2, 4);
                //cout << "\"" << temp << "\" ";
                //cout << "\"" << (float)strtol(temp.c_str(), 0, 16)/100 << "\"" << std::endl;

                Rooms[i].setSetPoint((float)strtol(temp.c_str(), 0, 16)/100);
            }
            handled = true;

        }
        if      (fields[6].compare("2309") == 0 && fields[8].length() < 10)
        {
            auto it = HR82s.find(fields[3]);

            if  (it == HR82s.end())
            {
                HR82s.insert (std::pair<std::string, HR82>(fields[3], HR82()));
            }

            std::string temp = fields[8].substr(2, 4);
            HR82s[fields[3]].setSetPoint((float)strtol(temp.c_str(), 0, 16)/100);
            HR82s[fields[3]].setName(fields[3]);
        }
//045  I --- 01:048853 --:------ 01:048853 30C9 021 0008380107800207430308020407DE050742060771
        else if      (fields[6].compare("30C9") == 0 && fields[8].length() > 10)
        {
            //cout << "Temparature" << std::endl;

            for (int i=0; i<atoi(fields[7].c_str())/3; i++)
            {
                std::string temp = fields[8].substr(i*6+2, 4);
                //cout << "\"" << temp << "\" ";
                //cout << "\"" << (float)strtol(temp.c_str(), 0, 16)/100 << "\"" << std::endl;

                Rooms[i].setTemp((float)strtol(temp.c_str(), 0, 16)/100);
            }
            handled = true;
        }
//045  I --- 04:065874 --:------ 04:065874 30C9 003 000766
        else if      (fields[6].compare("30C9") == 0 && fields[8].length() < 10)
        {
            //cout << "Temparature" << std::endl;

            for (int i=0; i<atoi(fields[7].c_str())/3; i++)
            {
                std::string temp = fields[8].substr(i*6+2, 4);
                //cout << "\"" << temp << "\" ";
                //cout << "\"" << (float)strtol(temp.c_str(), 0, 16)/100 << "\"" << std::endl;
            }
            handled = true;

            auto it = HR82s.find(fields[3]);

            if  (it == HR82s.end())
            {
                HR82s.insert (std::pair<std::string, HR82>(fields[3], HR82()));
            }

            std::string temp = fields[8].substr(2, 4);
            HR82s[fields[3]].setTemp((float)strtol(temp.c_str(), 0, 16)/100);
            HR82s[fields[3]].setName(fields[3]);

        }
//045 RP --- 01:048853 30:259525 --:------ 0004 022 0600536861636B000000000000000000000000000000
        else if      (fields[6].compare("0004") == 0 && fields[8].length() > 10)
        {
            int zone=atoi(fields[8].substr(0,2).c_str());

            char buffer[20];

            for (int i=2; i < atoi(fields[7].c_str()); i++)
            {
                buffer[i-2] = strtol(fields[8].substr(i*2,2).c_str(), 0, 16);    
            }

            Rooms[zone].setName(buffer);

            handled = true;
        }

//045 RP --- 01:048853 30:259525 --:------ 000A 006 051001F40DAC
//TODO:045  I --- 01:048853 --:------ 01:048853 000A 042 001001F40898011001F40834021001F40DAC031001F40DAC041001F40DAC051001F40DAC061001F40DAC

        else if      (fields[6].compare("000A") == 0 && fields[8].length() > 10)
        {
            int zone = atoi(fields[8].substr(0,2).c_str());
            float minTemp = (float)strtol(fields[8].substr(4,4).c_str(), 0, 16)/100;
            float maxTemp = (float)strtol(fields[8].substr(8,4).c_str(), 0, 16)/100;

            std::cout << "Zone #" << zone << ": Min:" << minTemp << " Max:" << maxTemp << std::endl;
            handled = true;
        }
//DateTime
//074  W --- 30:259525 01:048853 --:------ 313F 009 00600017150A0907DF]
//TODO:045  I --- 01:048853 30:259525 --:------ 313F 009 00FC8017750A0907DF]
        else if      (fields[6].compare("313F") == 0 && fields[8].length() > 10)
        {
            int sec = strtol(fields[8].substr(4,2).c_str(), 0, 16);
            int min = strtol(fields[8].substr(6,2).c_str(), 0, 16);
            int hour = strtol(fields[8].substr(8,2).c_str(), 0, 16) % 192;
            int day = strtol(fields[8].substr(10,2).c_str(), 0, 16);
            int month = strtol(fields[8].substr(12,2).c_str(), 0, 16);
            int year = strtol(fields[8].substr(14,4).c_str(), 0, 16);

            std::cout << "Time " << hour << ":" << min << ":" <<  sec << std::endl;
            std::cout << "Date " << day << "-" << month << "-" << year << std::endl; 
            handled = true;
        }
//050  I --- 04:067426 --:------ 01:048853 12B0 003 050000
        else if      (fields[6].compare("12B0") == 0)
        {   
            int zone = strtol(fields[8].substr(0,2).c_str(), 0, 16); 

            auto it = HR82s.find(fields[3]);

            if  (it == HR82s.end())
            {
                HR82s.insert (std::pair<std::string, HR82>(fields[3], HR82()));
            }

            HR82s[fields[3]].setZone(zone);
            HR82s[fields[3]].setName(fields[3]);

            handled = true;
        }
        else
        {
        }

        std::cout << currentTime() << (handled == true ? " v " : " x ") << "[" << msg << "]" << std::endl;
        //print( fields );
        //cout << fields.size() << " fields.\n";
    }

    return 0;
}
