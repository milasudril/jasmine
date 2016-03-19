#ifdef __WAND__
target[name[jasmine.h] type[include]]
dependency[jasmine.o]
#endif

#ifndef JASMINE_H
#define JASMINE_H

#include <cstdint>
#include <cstddef>

/**The Jasmine class. Waveform data is stored in channel-major order. That is
 * for stereo data, the layout is LLLL... and then RRRR...
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
			,const char* const* ports_out)
			{init(client_name,ports_in,ports_out,default_error_handler);}

		Jasmine(const char* client_name,const char* const* ports_in
			,const char* const* ports_out
			,ErrorHandler on_error)
			{init(client_name,ports_in,ports_out,on_error);}

		~Jasmine();

		size_t write(const float* data,size_t n_frames) noexcept;
		size_t read(float* data,size_t n_frames) const noexcept;
		float sampleRateGet() const noexcept;

	private:
		struct Impl;
		Impl* m_impl;

		static void default_error_handler(const ErrorMessage& message);
		void init(const char* client_name
			,const char* const* ports_in,const char* const* ports_out
			,ErrorHandler handler);
	};

#endif
