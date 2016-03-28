#ifdef __WAND__
target
	[
	platform[;GNU/Linux]
	name[jasmine.o]
	type[object]
	dependency[jack;external]
	dependency[pthread;external]
	]

target
	[
	platform[;Windows]
	name[jasmine.o]
	type[object]
	dependency[jack;external]
	]
#endif

#include "jasmine.h"
#include <jack/jack.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

//	Internal stuff
namespace {
#if ( _WIN32 || _WIN64 )
#include <windows.h>

class Event
	{
	public:
		Event(Jasmine::ErrorHandler on_error):
			m_handle(CreateEvent(NULL,FALSE,FALSE,NULL))
			{
			if(m_handle==NULL)
				{
				Jasmine::ErrorMessage message;
				FormatMessageA(
					 FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS
					,GetLastError()
					,0
					,message.message
					,message.MESSAGE_SIZE
					,NULL);
				on_error(message);
				}
			}

		void wait() noexcept
			{WaitForSingleObject(m_handle,INFINITE);}

		void set() noexcept
			{SetEvent(m_handle);}

		~Event()
			{CloseHandle(m_handle);}

	private:
		HANDLE m_handle;
	};

#else
#include <pthread.h>
class Event
	{
	public:
		Event(Jasmine::ErrorHandler on_error):m_signaled(0)
			{
			if(pthread_cond_init(&m_cond,NULL)!=0)
				{
				Jasmine::ErrorMessage message;
				strcpy(message.message,"Failed to create a conditional variable.");
				on_error(message);
				}
			if(pthread_mutex_init(&m_mutex,NULL)!=0)
				{
				Jasmine::ErrorMessage message;
				strcpy(message.message,"Failed to create a mutex.");
				pthread_cond_destroy(&m_cond);
				on_error(message);
				}
			}

		void wait() noexcept
			{
			pthread_mutex_lock(&m_mutex);
			while(!m_signaled)
				{pthread_cond_wait(&m_cond,&m_mutex);}
			m_signaled=0;
			pthread_mutex_unlock(&m_mutex);
			}

		void set() noexcept
			{
			pthread_mutex_lock(&m_mutex);
			pthread_cond_signal(&m_cond);
			m_signaled=1;
			pthread_mutex_unlock(&m_mutex);
			}

		~Event()
			{
			pthread_mutex_destroy(&m_mutex);
			pthread_cond_destroy(&m_cond);
			}

	private:
		pthread_cond_t m_cond;
		pthread_mutex_t m_mutex;
		volatile bool m_signaled;
	};
#endif

class BufferIn
	{
	public:
		inline BufferIn(unsigned int n_channels,unsigned int buffer_size_min
			,Jasmine::ErrorHandler on_error);
		inline ~BufferIn();

		inline unsigned int writeByChannel(const float* data,unsigned int n_frames
			,unsigned int n_channels_in,unsigned int channel_out_first) noexcept;

		inline unsigned int writeByFrame(const float* data,unsigned int n_frames
			,unsigned int n_channels_in,unsigned int channel_out_first) noexcept;

		inline unsigned int read(float* buffer,unsigned int n_frames,unsigned int channel)
			noexcept;

		inline unsigned int frameOffsetAdvance(unsigned int N) noexcept;

		bool done() noexcept
			{
			if(m_frame_offset==m_n_frames_total)
				{
				m_frame_offset=0;
				return 1;
				}
			return 0;
			}

		bool atBegin() noexcept
			{return m_frame_offset==0;}

	private:
		float* m_buffer;
		unsigned int m_frame_offset;
		unsigned int m_buffer_size_min;
		unsigned int m_n_frames_total;
		unsigned int m_n_frame_capacity;
		unsigned int m_n_channels;
	};

BufferIn::BufferIn(unsigned int n_channels,unsigned int buffer_size_min
	,Jasmine::ErrorHandler on_error):
	 m_buffer(nullptr),m_frame_offset(0),m_buffer_size_min(buffer_size_min)
	,m_n_frames_total(buffer_size_min),m_n_frame_capacity(0),m_n_channels(n_channels)
	{
	auto N=buffer_size_min*n_channels*sizeof(float);
	m_buffer=static_cast<float*>(malloc(N));
	if(m_buffer==nullptr && N!=0)
		{
		Jasmine::ErrorMessage msg;
		strcpy(msg.message,"It is not possible to allocate memory for another buffer.");
		on_error(msg);
		}
	memset(m_buffer,0,N);
	}

unsigned int BufferIn::writeByChannel(const float* data,unsigned int n_frames
	,unsigned int n_channels_in,unsigned int channel_out_first) noexcept
	{
	auto n_frames_tot=std::max(n_frames,m_buffer_size_min);

	auto n_channels_out_tot=m_n_channels;
	if(n_channels_in+channel_out_first > n_channels_out_tot)
		{return 0;}

	auto capacity=m_n_frame_capacity;
	float* buffer=m_buffer;
	if(n_frames_tot > capacity)
		{
		auto temp=realloc(buffer,n_frames_tot*n_channels_out_tot*sizeof(float));
		if(temp==nullptr)
			{return 0;}
		capacity=n_frames;
		m_n_frame_capacity=n_frames_tot;
		buffer=static_cast<float*>(temp);
		m_buffer=buffer;
		}

	auto pos=buffer+channel_out_first*n_frames_tot;
	memset(pos,0,n_frames_tot*n_channels_in*sizeof(float));
	memcpy(pos,data,n_frames*n_channels_in*sizeof(float));
	m_frame_offset=0;
	m_n_frames_total=n_frames_tot;
	return n_frames;
	}

unsigned int BufferIn::writeByFrame(const float* data,unsigned int n_frames
	,unsigned int n_channels_in,unsigned int channel_out_first) noexcept
	{
	auto n_frames_tot=std::max(n_frames,m_buffer_size_min);

	auto n_channels_out_tot=m_n_channels;
	if(n_channels_in+channel_out_first > n_channels_out_tot)
		{return 0;}

	auto capacity=m_n_frame_capacity;
	float* buffer=m_buffer;
	if(n_frames_tot > capacity)
		{
		auto temp=realloc(buffer,n_frames_tot*n_channels_out_tot*sizeof(float));
		if(temp==nullptr)
			{return 0;}
		capacity=n_frames;
		m_n_frame_capacity=n_frames_tot;
		buffer=static_cast<float*>(temp);
		m_buffer=buffer;
		}

		{
		auto position=buffer+channel_out_first*n_frames_tot;
		auto n=n_frames;
		auto n_ch=std::min(n_channels_in,n_channels_out_tot-channel_out_first);
		memset(position,0,n_frames_tot*n_ch*sizeof(float));
		while(n!=0)
			{
			for(unsigned int k=0;k<n_ch;++k)
				{
				*(position + k*n_frames_tot)=*data;
				++data;
				}
			if(n_ch < n_channels_in)
				{data+=n_channels_in - n_ch;}
			++position;
			--n;
			}
		}

	m_frame_offset=0;
	m_n_frames_total=n_frames_tot;
	return n_frames;
	}

BufferIn::~BufferIn()
	{
	free(m_buffer);
	}

unsigned int BufferIn::read(float* buffer,unsigned int n_frames
	,unsigned int channel) noexcept
	{
	auto n_read=m_frame_offset;
	auto n_frames_tot=m_n_frames_total;

	auto source=m_buffer+n_read + n_frames_tot*channel;
	auto n_remaining=std::min(n_frames, n_frames_tot - n_read);

	memset(buffer,0,n_frames*sizeof(float));

	if(n_remaining==0)
		{return 0;}
	memcpy(buffer,source,n_remaining*sizeof(float));
	return n_remaining;
	}

unsigned int BufferIn::frameOffsetAdvance(unsigned int N) noexcept
	{
	auto n_read=m_frame_offset;
	auto n_frames_tot=m_n_frames_total;

	auto n_remaining=std::min(N,n_frames_tot - n_read);

	n_read+=n_remaining;
	if(n_read==n_frames_tot)
		{memset(m_buffer,0,n_frames_tot*m_n_channels*sizeof(float));}
	m_frame_offset+=n_remaining;
	return n_remaining;
	}
}




class Jasmine::Impl
	{
	public:
		inline Impl(const char* client_name,const char* const* ports_in
			,const char* const* ports_out
			,unsigned int buffer_size
			,ErrorHandler on_error);

		inline ~Impl();

		inline int dataProcess(jack_nframes_t N) noexcept;
		inline void writeByChannel(const float* data,unsigned int n_frames
			,unsigned int n_channels_in,unsigned int channel_out_first);
		inline void writeByFrame(const float* data,unsigned int n_frames
			,unsigned int n_channels_in,unsigned int channel_out_first);
		inline void playbackReadyWait() noexcept;


	private:
		static int data_process(jack_nframes_t N,void* jasmine) noexcept;

		void ports_destroy(std::vector<jack_port_t*>& ports);
		void cleanup();

		jack_client_t* m_client;

		std::vector<jack_port_t*> m_ports_in;
		std::vector<jack_port_t*> m_ports_out;

		std::unique_ptr<BufferIn> m_buffers_in[2];
		Event m_playback_ready;
		bool m_playback_done;
	};

void Jasmine::default_error_handler(const ErrorMessage& message)
	{
	fprintf(stderr,"Jasmine: %s\n",message.message);
	abort();
	}

int Jasmine::Impl::data_process(jack_nframes_t N,void* jasmine) noexcept
	{return reinterpret_cast<Jasmine::Impl*>(jasmine)->dataProcess(N);}

Jasmine::Impl::Impl(const char* client_name,const char* const* ports_in
	,const char* const* ports_out
	,unsigned int buffer_size
	,ErrorHandler on_error):m_playback_ready(on_error),m_playback_done(1)
	{
	jack_status_t status;
	auto client=jack_client_open(client_name,JackNoStartServer,&status,"default");
	if(client==NULL)
		{
		ErrorMessage msg;
		strcpy(msg.message,"It is not possible to connect to JACK. Make sure "
			"there is a running JACK server.");
		on_error(msg);
		}

	//	Add input ports
	if(ports_in!=nullptr)
		{
		while(*ports_in!=nullptr)
			{
			auto port=jack_port_register(client,*ports_in
				,JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput,0);
			if(port==NULL)
				{
				cleanup();
				ErrorMessage msg;
				strcpy(msg.message,"It is not add another input port to the client.");
				on_error(msg);
				}

			m_ports_in.push_back(port);
			++ports_in;
			}
		}

	//	Add output ports
	if(ports_out!=nullptr)
		{
		while(*ports_out!=nullptr)
			{
			auto port=jack_port_register(client,*ports_out
				,JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput,0);

			if(port==NULL)
				{
				cleanup();
				ErrorMessage msg;
				strcpy(msg.message,"It is not add another output port to the client.");
				on_error(msg);
				}

			m_ports_out.push_back(port);
			++ports_out;
			}
		}

	m_buffers_in[0].reset(new BufferIn(static_cast<unsigned int>(m_ports_out.size()),buffer_size,on_error));
	m_buffers_in[1].reset(new BufferIn(static_cast<unsigned int>(m_ports_out.size()),buffer_size,on_error));

	jack_set_process_callback(client,data_process,this);
	jack_activate(client);
	m_client=client;
	}

void Jasmine::Impl::ports_destroy(std::vector<jack_port_t*>& ports)
	{
	auto client=m_client;
	auto ptr_begin=ports.data();
	auto ptr_end=ports.data()+ports.size();
	while(ptr_begin!=ptr_end)
		{
		--ptr_end;
		jack_port_unregister(client,*ptr_end);
		}
	}

void Jasmine::Impl::cleanup()
	{
	jack_deactivate(m_client);
	ports_destroy(m_ports_out);
	ports_destroy(m_ports_in);
	jack_client_close(m_client);
	}

Jasmine::Impl::~Impl()
	{
	cleanup();
	}

void Jasmine::Impl::writeByChannel(const float* data,unsigned int n_frames
	,unsigned int n_channels_in,unsigned int channel_out_first)
	{
	auto buffer=m_buffers_in[0].get();
	buffer->writeByChannel(data,n_frames,n_channels_in,channel_out_first);
	}

void Jasmine::Impl::writeByFrame(const float* data,unsigned int n_frames
	,unsigned int n_channels_in,unsigned int channel_out_first)
	{
	auto buffer=m_buffers_in[0].get();
	buffer->writeByFrame(data,n_frames,n_channels_in,channel_out_first);
	}

void Jasmine::Impl::playbackReadyWait() noexcept
	{
	m_playback_ready.wait();
	}


inline int Jasmine::Impl::dataProcess(jack_nframes_t N) noexcept
	{
	//	Write data to output port
		{
		if(m_playback_done)
			{
			std::swap(m_buffers_in[0],m_buffers_in[1]);
			m_playback_done=0;
			m_playback_ready.set();
			}

		auto buffer_in_front=m_buffers_in[1].get();
		auto ports_out_begin=m_ports_out.data();
		auto ports_out_end=ports_out_begin + m_ports_out.size();
		auto ch=0;
		while( ports_out_begin!=ports_out_end )
			{
			float* buffer_out=static_cast<float*>
				(jack_port_get_buffer(*ports_out_begin,N));

			buffer_in_front->read(buffer_out,N,ch);
			++ch;
			++ports_out_begin;
			}
		buffer_in_front->frameOffsetAdvance(N);
		if(buffer_in_front->done())
			{m_playback_done=1;}
		}

	return 0;
	}

void Jasmine::init(const char* client_name,const char* const* ports_in
	,const char* const* ports_out
	,unsigned int buffer_size
	,ErrorHandler on_error)
	{
	m_impl=new Impl(client_name,ports_in,ports_out,buffer_size,on_error);
	}

Jasmine::~Jasmine()
	{delete m_impl;}

void Jasmine::playbackReadyWait() noexcept
	{
	m_impl->playbackReadyWait();
	}

void Jasmine::writeByChannel(const float* data,unsigned int n_frames
	,unsigned int n_channels_in,unsigned int channel_out_first)
	{
	m_impl->writeByChannel(data,n_frames,n_channels_in,channel_out_first);
	}

void Jasmine::writeByFrame(const float* data,unsigned int n_frames
	,unsigned int n_channels_in,unsigned int channel_out_first)
	{
	m_impl->writeByFrame(data,n_frames,n_channels_in,channel_out_first);
	}
