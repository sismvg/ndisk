
#include <single_archive_dispath.hpp>


memory_block default_alloc_archive_memory(size_t size)
{
	return memory_block(new unsigned char[size], size);
}

void default_free_archive_memory(void* ptr, size_t size)
{
	delete [] ptr;
}

void default_free_archive_memory(const_memory_block block)
{
	default_free_archive_memory(
		const_cast<void*>(block.buffer), block.size);
}