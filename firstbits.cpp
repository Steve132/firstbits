#include<cstring>
#include<cstdint>
#include<cctype>
#include<iostream>
#include<fstream>
#include<algorithm>
#include "firstbits.h"

#include<sys/mman.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const uint32_t WRITER_MODE=0xFFFFFFFF;

inline static void lowermod(address_t& add)
{
	for(int i=1;i<SIZE_ADDRESS;i++)		//don't mod leading character
	{
		add.data[i]=std::tolower(add.data[i]);
	}
}
bool operator<(address_t a,address_t b)
{
	lowermod(a);
	lowermod(b);
	return memcmp(reinterpret_cast<const char*>(&a),reinterpret_cast<const char*>(&b),SIZE_ADDRESS) < 0;
}
bool search_comparator(address_t a,address_t b)
{
	size_t na=strlen(reinterpret_cast<const char*>(&a));
	size_t n=strlen(reinterpret_cast<const char*>(&b));
	na=na < SIZE_ADDRESS ? na : SIZE_ADDRESS;
	n=n < na ? n : na;
	lowermod(a);
	lowermod(b);
	return memcmp(reinterpret_cast<const char*>(&a),reinterpret_cast<const char*>(&b),n) < 0;
}
bool meta_comparator(address_t a,address_t b)
{
	return a.meta < b.meta;
}


struct block_t
{
	std::uint16_t num_addresses;
	address_t first;	//is a dynamic length array of addresses in contiguous memory.  Zero-padded
};

template<class IntType>
static inline IntType read_le(std::istream& in)
{
	IntType dout=0;
	for(unsigned int i=0;i<sizeof(IntType);i++)
	{
		dout|=static_cast<IntType>(in.get()) << (8*i);
	}
	return dout;
}
template<class IntType>
static inline void write_le(std::ostream& out,const IntType& it)
{
	for(unsigned int i=0;i<sizeof(IntType);i++)
	{
		out.put((it >> (8*i)) & 0xFF);
	}
}
static inline int char2hashdex(char a)
{
	if(std::isalpha(a))
	{
		return std::tolower(a)-'a'+10;
	}
	else if(std::isdigit(a))
	{
		return a-'0';
	}
	return 0;
}


inline std::uint64_t firstbits_t::hashbucket(const address_t& bitsearch) const
{
	uint64_t hashindex=0;
	for(uint_fast8_t i=0;i<mdata.num_characters_per_block;i++)
	{
		hashindex*=36;
		hashindex+=char2hashdex(bitsearch.data[1+i]);
	}
	return hashindex;
}

	
void firstbits_t::create_file(const std::string& fn,const metadata_t& md)
{
	static const size_t zerosize=1 << 28;
	std::ofstream outfile(fn.c_str(),std::ofstream::binary | std::ofstream::out);
	write_le(outfile,md.num_blocks);
	write_le(outfile,md.max_addresses_per_block);
	write_le(outfile,md.num_characters_per_block);
	write_le<uint32_t>(outfile,0);//set the genesis block
	size_t blocksize=md.max_addresses_per_block*sizeof(address_t)+sizeof(std::uint16_t);
	size_t totalsize=md.num_blocks*blocksize;
	
	
	char* zbuf=(char*)malloc(zerosize);
	memset(zbuf,0,zerosize);
	
	for(size_t i=0;i<totalsize / zerosize;i++)
	{
		outfile.write(zbuf,zerosize);
	}
	outfile.write(zbuf,totalsize % zerosize);
	outfile.close();
}
	
inline int firstbits_t::safemsync(void* addr,size_t len,int flags)
{
/*	size_t addrl=(size_t)addr;
	size_t addrp=addrl & ~(pagesize-1);
	len+=addrl-addrp;
	return msync((void*)addrp,len,flags);
*/
	return 0;
}


	//loading constructor
firstbits_t::firstbits_t(const std::string& filename,const metadata_t& md)
{
	std::ifstream indata(filename.c_str(),std::ifstream::binary | std::ifstream::in);
	if(!indata)
	{
		indata.close();
		create_file(filename,md);
		indata.open(filename.c_str(),std::ifstream::binary | std::ifstream::in);
	}

	mdata.num_blocks=read_le<std::uint64_t>(indata);
	mdata.max_addresses_per_block=read_le<std::uint16_t>(indata);
	mdata.num_characters_per_block=read_le<std::uint8_t>(indata);
	indata.close();
	block_threadstate.reset(new std::atomic<std::uint32_t>[mdata.num_blocks]);
	blocksize=mdata.max_addresses_per_block*sizeof(address_t)+sizeof(std::uint16_t);
	size_t len=sizeof(metadata_t)+sizeof(uint32_t)+mdata.num_blocks*blocksize;
	
	backendfd=open(filename.c_str(),O_RDWR | O_NOATIME);

	void* fb=mmap(NULL,len,PROT_READ | PROT_WRITE, MAP_SHARED,backendfd,0);
	pagesize=getpagesize();
	
	filebeginning=static_cast<char*>(fb);
	
	char* curpos=filebeginning;
	curpos+=sizeof(mdata);
	lastblockheight=reinterpret_cast<uint32_t*>(curpos);
	curpos+=sizeof(uint32_t);
	
	tablebeginning=curpos;
}

std::vector<address_t> firstbits_t::get_firstbits(const std::string& bitsearch) const
{
	if(bitsearch.size() < 5 || bitsearch.size() > SIZE_ADDRESS)
	{
		return std::vector<address_t>();
	}
	
	address_t searchbits(bitsearch);

	std::uint64_t hashindex=hashbucket(searchbits);
	std::atomic<std::uint32_t>* readerstate=&block_threadstate[hashindex];
			
	uint32_t prev=WRITER_MODE;
	while(readerstate->compare_exchange_strong(prev,WRITER_MODE)); //while it's writing, loop
	++(*readerstate);
	
	const block_t* thisblock=(const block_t*)(tablebeginning+hashindex*blocksize);
	
	const address_t* first=&thisblock->first;
	const address_t* last=first+thisblock->num_addresses;
	const address_t *outbegin=std::lower_bound(first,last,searchbits,search_comparator);
	const address_t *outend=std::upper_bound(first,last,searchbits,search_comparator);
	
	--(*readerstate);
	std::vector<address_t> out(outbegin,outend);
	std::sort(out.begin(),out.end(),meta_comparator);
	return out;
}
	
void firstbits_t::insert_address(const address_t& address)		
{
	std::uint64_t hashindex=hashbucket(address);
	std::atomic<std::uint32_t>* readerstate=&block_threadstate[hashindex];		
	
	uint32_t prev=0;
	while(!readerstate->compare_exchange_strong(prev,WRITER_MODE)); //while it's reading or writing, loop

	block_t* thisblock=(block_t*)(tablebeginning+hashindex*blocksize);
	
	address_t* first=&thisblock->first;
	address_t* last=first+thisblock->num_addresses;
	address_t *position=std::lower_bound(first,last,address);
	
	if(strncmp(reinterpret_cast<const char*>(&position->data[0]),reinterpret_cast<const char*>(&address.data[0]),SIZE_ADDRESS)==0)//Don't insert duplicates! only update metadata
	{
		position->meta=std::max(address.meta,position->meta);
	}
	else if(thisblock->num_addresses != this->mdata.max_addresses_per_block) 
	{
		*(last)=address;
		std::rotate(position,last,last+1);
		thisblock->num_addresses+=1;
	}
	
	safemsync(thisblock,blocksize,MS_ASYNC);
	
	*readerstate=0;
}

