#ifdef __WAND__
target[name[jasmine.h] type[include]]
dependency[jasmine.o]
#endif

#ifndef JASMINE_H
#define JASMINE_H

#include <cstdint>
#include <cstddef>

/**The Jasmine class.
*/
class Jasmine
	{
	public:
		struct ErrorMessage
			{
			static constexpr uint32_t MESSAGE_LENGTH=1024;
			char message[MESSAGE_LENGTH];
			};

		/**Called wehn constructor fails. The default function prints the message
		 * and calls abort(3), but it can also throw the message object as an
		 * exception or package it into another object and throw that. However,
		 * it must not return.
		*/
		typedef void (*ErrorHandler [[noreturn]])(const ErrorMessage& message);

		Jasmine(const char* client_name
			,const char* const* ports_in
			,const char* const* ports_out
			,unsigned int buffer_size)
			{init(client_name,ports_in,ports_out,buffer_size,default_error_handler);}

		Jasmine(const char* client_name,const char* const* ports_in
			,const char* const* ports_out
			,unsigned int buffer_size
			,ErrorHandler on_error)
			{init(client_name,ports_in,ports_out,buffer_size,on_error);}

		~Jasmine();

		void playbackReadyWait() noexcept;

		void writeByChannel(const float* data,unsigned int n_frames
			,unsigned int n_channels_in,unsigned int channel_out_first);

		void writeByFrame(const float* data,unsigned int n_frames
			,unsigned int n_channels_in,unsigned int channel_out_first);

		void read(float* data,unsigned int n_frames,unsigned int channel) const noexcept;

		float sampleRateGet() const noexcept;

	private:
		struct Impl;
		Impl* m_impl;

		static void default_error_handler(const ErrorMessage& message);
		void init(const char* client_name
			,const char* const* ports_in,const char* const* ports_out
			,unsigned int buffer_size
			,ErrorHandler handler);
	};

#endif
