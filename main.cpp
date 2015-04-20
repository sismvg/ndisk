
#include <thread>
#include <string>
#include <iostream>

//user header

#include <ndisk_io.hpp>
#include <ndisk_machine.hpp>

using namespace std;

int main(int argc,char* argv[])
{
//-f config_file_path

	cout<<"load config"<<endl;

	cout<<"start io service"<<endl;

	machine ma;
	thread io_service(mine);

	cout<<"start control service"<<endl;
	cout<<"Finish,Input help to list commands"<<endl;

	while(true)
	{
		string cmd;
		cin>>cmd;
		if(cmd=="exit")
		{
			cout<<"exit"<<endl;
			io_service.join();
			break;
		}
		//do something;
	}

return 0;
}
