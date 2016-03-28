#ifdef __WAND__
target[platform[;GNU/Linux]name[jasmine-player] type[application] dependency[sndfile;external]]
target[platform[;Windows]name[jasmine-player.exe] type[application]  dependency[sndfile;external]]
#endif

#include "jasmine.h"
#include <sndfile.h>
#include <memory>
#include <string>
#include <vector>
#include <cstring>

bool filenameGet(FILE* src,std::string& ret)
	{
	ret.clear();
	int ch_in;
	while( (ch_in=getc(src))!=EOF )
		{
		switch(ch_in)
			{
			case '\r':
				break;
			case '\n':
				return 1;

			default:
				ret+=ch_in;
			}
		}

	if(ret.size()!=0)
		{return 1;}
	return 0;
	}

const char* ports[]={"Left","Right",nullptr};

std::unique_ptr<SNDFILE,decltype(&sf_close)> sourceOpen(const char* filename
	,SF_INFO& info)
	{
	return {sf_open(filename, SFM_READ,&info),sf_close};
	}

int main()
	{
	Jasmine player("Jasmine",nullptr,ports,0);
	std::string filename;
	while(filenameGet(stdin,filename))
		{
		SF_INFO info;
		auto source=sourceOpen(filename.c_str(),info);

		if(source.get()==nullptr)
			{continue;}

		printf("Playing %s\n",filename.c_str());
 		auto N=48000;
		auto n=N;
		std::vector<float> buffer(N*info.channels);
		do
			{
			n=sf_readf_float(source.get(),buffer.data(),N);
			player.playbackReadyWait();
			player.writeByFrame(buffer.data(),n,info.channels,0);
			}
		while(n==N);
		}
	}
