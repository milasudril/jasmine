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
#include <vector>

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
		Event(Jasmine::ErrorHandler on_error)
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

class Jasmine::Impl
	{
	public:
		inline Impl(const char* client_name,const char* const* ports_in
			,const char* const* ports_out
			,ErrorHandler on_error);

		inline ~Impl();

		inline int dataProcess(jack_nframes_t N) noexcept;

	private:
		static int data_process(jack_nframes_t N,void* jasmine) noexcept;

		void ports_destroy(std::vector<jack_port_t*>& ports);
		void cleanup();

		jack_client_t* m_client;
		std::vector<jack_port_t*> m_ports_in;
		std::vector<jack_port_t*> m_ports_out;

		Event m_data_ready;
		std::vector<float> m_data_in_a;
		std::vector<float> m_data_in_b;

		float* m_data_in_current;
		float* m_data_in_base;
		float* m_data_in_other;
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
	,ErrorHandler on_error):m_data_ready(on_error)
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

		m_data_in_a.resize( m_ports_out.size() * 48000);
		m_data_in_b.resize( m_ports_out.size() * 48000);
		m_data_in_base=m_data_in_a.data();
		m_data_in_current=m_data_in_base;
		m_data_in_other=m_data_in_b.data();
		}

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

inline int Jasmine::Impl::dataProcess(jack_nframes_t N) noexcept
	{
	//	Write data to output ports
		{
		auto ports_out_begin=m_ports_out.data();
		auto ports_out_end=ports_out_begin + m_ports_out.size();
		while( ports_out_begin!=ports_out_end )
			{
			float* buffer_out=(float*)jack_port_get_buffer(*ports_out_begin,N);

			++ports_out_begin;
			}
		}

	return 0;
	}


void Jasmine::init(const char* client_name,const char* const* ports_in
	,const char* const* ports_out
	,ErrorHandler on_error)
	{
	m_impl=new Impl(client_name,ports_in,ports_out,on_error);
	}

Jasmine::~Jasmine()
	{delete m_impl;}
