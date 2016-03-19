#ifdef __WAND__
target[name[jasmine.h] type[include]]
dependency[jasmine.o]
#endif

#ifndef JASMINE_H
#define JASMINE_H

class Jasmine
	{
	public:
		Jasmine(const char* client_name,const char* const* ports_in
			,const char* const* ports_out);
		~Jasmine();

		void write(const float* data,size_t n_frames);
		void read(float* data,size_t n_frames);

	private:
		struct Impl;
		Impl* m_impl;
	};

#endif
