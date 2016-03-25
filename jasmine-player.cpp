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

const char* ports[]={"Left",nullptr};

void outputTranspose(const float* source,float* dest
	,unsigned int N_ch_in,unsigned int N_ch_out,unsigned int n_frames)
	{
	unsigned int n_fetch=std::min(N_ch_in,N_ch_out);
	for(unsigned int k=0;k<n_fetch;++k)
		{
		auto ptr_out=dest + k*n_frames;
		auto N=n_frames;
		auto ptr_in=source+k;
		while(N!=0)
			{
			*ptr_out=*ptr_in;
			++ptr_out;
			ptr_in+=N_ch_in;
			--N;
			}
		}
	}

int main()
	{
	Jasmine player("Jasmine",nullptr,ports,48000);
	std::string filename;
	while(filenameGet(stdin,filename))
		{
		SF_INFO info;
		std::unique_ptr<SNDFILE,decltype(&sf_close)> source
			{sf_open(filename.c_str(), SFM_READ,&info),sf_close};

		printf("Playing %s\n",filename.c_str());
		auto N=48000;
		auto N_ch_out=player.outputChannelsCount();
		auto n=N;
		std::vector<float> buffer(N*info.channels);
		do
			{
			n=sf_read_float(source.get(),buffer.data(),N);
			player.write(buffer.data(),n);
			}
		while(n==N);
		player.write(buffer.data(),0);
		player.write(buffer.data(),0);
		}
	}
