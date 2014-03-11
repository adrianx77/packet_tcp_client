// test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "../../src/tcpclient.h"
#include <conio.h>
int _tmain(int argc, _TCHAR* argv[])
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

