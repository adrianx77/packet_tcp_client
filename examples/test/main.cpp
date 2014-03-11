//
//  main.cpp
//  test
//
//  Created by 刘金鑫 on 14-3-11.
//
//
#ifndef _WIN32
#include <stdio.h>
#else
#include "stdafx.h"
#include <conio.h>
#endif
#include "../../src/tcpclient.h"

#ifndef _WIN32

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
int _kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF)
    {
        ungetc(ch, stdin);  
        return 1;  
    }  
    return 0;  
}
#define Sleep sleep
#define _getch getchar
#endif

int main(int argc, const char * argv[])
{
	boost::shared_ptr<adrianx::tcp_client> client(new adrianx::tcp_client());
	client->connect("10.10.10.6",8082);
	int key = 0;
    
	for (;;)
	{
		key = _kbhit();
		if(!key)
		{
			Sleep(1);
			continue;
		}
		key = _getch();
		//ESC key
		if(key==0x1B)
			break;
        
		if(key==' ')
		{
			std::cout<<" space key down"<<std::endl;
			char s[] = "asdfasdfasdf";
			client->send((uint8_t*)s,sizeof(s));
		}
	}
	client->close();
	return 0;

}

