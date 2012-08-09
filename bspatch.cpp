/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
//#include <err.h>
#include "bzlib.h"
#include <io.h>
#include <fcntl.h>
#include "sha1.h"

#include <sys/types.h>
typedef unsigned char u_char;
typedef signed int ssize_t;

extern "C" {
#include "lua.h"
}

template<class T1, class T2>
void err(int i, const char* str, T1 arg1, T2 arg2) {
	char lastErrorTxt[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError(),0,lastErrorTxt,1024,NULL);
	printf("%s",lastErrorTxt);
	printf(str, arg1, arg2);
	exit(i);
}
template<class T>
void err(int i, const char* str, T arg) {
	char lastErrorTxt[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError(),0,lastErrorTxt,1024,NULL);
	printf("%s",lastErrorTxt);
	printf(str, arg);
	exit(i);
}
void err(int i, const char* str) {
	char lastErrorTxt[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError(),0,lastErrorTxt,1024,NULL);
	printf("%s",lastErrorTxt);
	if (str!=NULL) {
		printf("%s",str);
	}
	exit(i);
}
template<class T>
void errx(int i, const char* str, T arg) {
	printf(str, arg);
	exit(i);
}
void errx(int i, const char* str) {
	printf("%s",str);
	exit(i);
}


static off_t offtin(u_char *buf)
{
	off_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

int bspatch(lua_State *L)
{
	FILE * f, * cpf, * dpf, * epf;
	BZFILE * cpfbz2, * dpfbz2, * epfbz2;
	int cbz2err, dbz2err, ebz2err;
	int fd;
	ssize_t oldsize,newsize;
	ssize_t bzctrllen,bzdatalen;
	u_char header[32],buf[8];
	u_char *old, *_new;
	off_t oldpos,newpos;
	off_t ctrl[3];
	off_t lenread;
	off_t i;

	const char* oldfile    = lua_tostring(L, 1);
	const char* newfile    = lua_tostring(L, 2);
	const char* patchfile  = lua_tostring(L, 3);

	/* Open patch file */
	if ((f = fopen(patchfile, "rb")) == NULL) {
		lua_pushfstring(L, "Unable to open patchfile %s", patchfile);
		lua_error(L);
	}

	/*
	File format:
		0	8	"BSDIFF40"
		8	8	X
		16	8	Y
		24	8	sizeof(newfile)
		32	X	bzip2(control block)
		32+X	Y	bzip2(diff block)
		32+X+Y	???	bzip2(extra block)
	with control block a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	if (fread(header, 1, 32, f) < 32) {
		if (feof(f)) {
			lua_pushliteral(L, "Corrupt patch");
			lua_error(L);
		}
		lua_pushliteral(L, "Unable to read patch file");
		lua_error(L);
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0) {
		lua_pushliteral(L, "Corrupt patch file");
		lua_error(L);
	}

	/* Read lengths from header */
	bzctrllen=offtin(header+8);
	bzdatalen=offtin(header+16);
	newsize=offtin(header+24);
	if((bzctrllen<0) || (bzdatalen<0) || (newsize<0)) {
		lua_pushliteral(L, "Corrupt patch file");
		lua_error(L);
	}

	/* Close patch file and re-open it via libbzip2 at the right places */
	if (fclose(f)) {
		lua_pushliteral(L, "fclose failed");
		lua_error(L);
	}
	if ((cpf = fopen(patchfile, "rb")) == NULL) {
		lua_pushfstring(L, "Unable to open patchfile %s", patchfile);
		lua_error(L);
	}
	if (fseek(cpf, 32, SEEK_SET)) {
		lua_pushliteral(L, "SEEK_SET failed");
		lua_error(L);
	}
	if ((cpfbz2 = BZ2_bzReadOpen(&cbz2err, cpf, 0, 0, NULL, 0)) == NULL) {
		lua_pushfstring(L, "BZ2_bzReadOpen, bz2err = %d", cbz2err);
		lua_error(L);
	}
	if ((dpf = fopen(patchfile, "rb")) == NULL) {
		lua_pushfstring(L, "Unable to open patchfile %s", patchfile);
		lua_error(L);
	}
	if (fseek(dpf, 32 + bzctrllen, SEEK_SET)) {
		lua_pushfstring(L, "fseeko(%s, %d)", patchfile,
		    (32 + bzctrllen));
		lua_error(L);
	}
	if ((dpfbz2 = BZ2_bzReadOpen(&dbz2err, dpf, 0, 0, NULL, 0)) == NULL) {
		lua_pushfstring(L, "BZ2_bzReadOpen, bz2err = %d", cbz2err);
		lua_error(L);
	}
	if ((epf = fopen(patchfile, "rb")) == NULL) {
		lua_pushfstring(L, "Unable to open patchfile %s", patchfile);
		lua_error(L);
	}
	if (fseek(epf, 32 + bzctrllen + bzdatalen, SEEK_SET)) {
		lua_pushfstring(L, "fseeko(%s, %d)", patchfile,
		    (32 + bzctrllen + bzdatalen));
		lua_error(L);
	}
	if ((epfbz2 = BZ2_bzReadOpen(&ebz2err, epf, 0, 0, NULL, 0)) == NULL) {
		lua_pushfstring(L, "BZ2_bzReadOpen, bz2err = %d", cbz2err);
		lua_error(L);
	}

	//org:
	//if(((fd=open(argv[1],O_RDONLY,0))<0) ||
	//	((oldsize=lseek(fd,0,SEEK_END))==-1) ||
	//	((old=malloc(oldsize+1))==NULL) ||
	//	(lseek(fd,0,SEEK_SET)!=0) ||
	//	(read(fd,old,oldsize)!=oldsize) ||
	//	(close(fd)==-1)) err(1,"%s",argv[1]);
	//new:
	//Read in chunks, don't rely on read always returns full data!
	if(((fd=open(oldfile,O_RDONLY|O_BINARY|O_NOINHERIT,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=(u_char*)malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0)) {
			lua_pushfstring(L, "Unable to open original file %s", oldfile);
	}
	int r=oldsize;
	while (r>0 && (i=read(fd,old+oldsize-r,r))>0) r-=i;
	if (r>0 || close(fd)==-1) {
		lua_pushfstring(L, "Error writing to/closing the original file %s", oldfile);
		lua_error(L);
	}

	if((_new=(u_char*)malloc(newsize+1))==NULL) {
		lua_pushliteral(L, "malloc failed");
		lua_error(L);
	}

	oldpos=0;newpos=0;
	while(newpos<newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			lenread = BZ2_bzRead(&cbz2err, cpfbz2, buf, 8);
			if ((lenread < 8) || ((cbz2err != BZ_OK) &&
			    (cbz2err != BZ_STREAM_END))) {
					lua_pushliteral(L, "Corrupt patch");
					lua_error(L);
				}
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>newsize) {
			lua_pushliteral(L, "Corrupt patch");
			lua_error(L);
		}

		/* Read diff string */
		lenread = BZ2_bzRead(&dbz2err, dpfbz2, _new + newpos, ctrl[0]);
		if ((lenread < ctrl[0]) ||
		    ((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END))) {
			lua_pushliteral(L, "Corrupt patch");
			lua_error(L);
		}

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				_new[newpos+i]+=old[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>newsize) {
			lua_pushliteral(L, "Corrupt patch\n");
			lua_error(L);
		}

		/* Read extra string */
		lenread = BZ2_bzRead(&ebz2err, epfbz2, _new + newpos, ctrl[1]);
		if ((lenread < ctrl[1]) ||
		    ((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END))) {
			lua_pushliteral(L, "Corrupt patch\n");
			lua_error(L);
		}

		/* Adjust pointers */
			newpos+=ctrl[1];
			oldpos+=ctrl[2];
		};

	/* Clean up the bzip2 reads */
	BZ2_bzReadClose(&cbz2err, cpfbz2);
	BZ2_bzReadClose(&dbz2err, dpfbz2);
	BZ2_bzReadClose(&ebz2err, epfbz2);
	if (fclose(cpf) || fclose(dpf) || fclose(epf)) {
		lua_pushliteral(L, "fclose failed");
		lua_error(L);
	}

	/* Write the new file */
	//org:
	//if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,0666))<0) ||
	//new:
	if(((fd=open(newfile,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0666))<0) ||
		(write(fd,_new,newsize)!=newsize) || (close(fd)==-1)) {
			lua_pushfstring(L, "Unable to open and/or write to %s", newfile);
			lua_error(L);
	}

	free(_new);
	free(old);

	return 0;
}
