/*
    Rhpc : R HPC environment
    Copyright (C) 2012-2021  Junji NAKANO and Ei-ji Nakama

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License,
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "../common/fakemaster.h"

/*
int WINAPI WinMain(HINSTANCE hInstance,
		   HINSTANCE hPrevInstance,
		   LPSTR lpCmdLine,
		   int nCmdShow)
*/
int main (int argc, char *argv[])
{
  char buf[FAKE_BUF_SZ];
  char *b;
  HANDLE hP;
  DWORD dwNumberOfBytesWritten;
  DWORD dwNumberOfBytesRead;
  char *envs;
  char *e;

  char pipename[FAKE_PATH_MAX];
  char msg[FAKE_BUF_SZ];

  /*  
  if (lpCmdLine == NULL){
  */
  if (argc != 2){
    snprintf(msg, sizeof(msg), "can't get pipename");
    MessageBox(NULL,
	       msg,
	       "Rhpc:fakemaster",
	       MB_OK);
    return 1;
  }
  else {
    /*
    strncpy(pipename, lpCmdLine, sizeof(pipename));
    */
    strncpy(pipename, argv[1], sizeof(pipename)-1);
  }
  
  memset(buf,0,sizeof(buf));
  b=buf;
  e=envs=GetEnvironmentStrings();
  if(envs == NULL) {
    MessageBox(NULL,
	       "Can't get environment strings",
	       "Rhpc:fakemaster",
	       MB_OK);
    return 1;
  }
  while(*e){
    if(0==strncmp(e, "PMI_", 4)){
      if(b-buf+strlen(e)+1+2> sizeof(buf)){
	MessageBox(NULL,
		   "so long environment strings\n"
		   "try change FAKE_BUF_SZ in fakemaster.h",
		   "Rhpc:fakemaster",
		   MB_OK);
	return 1;
      }
      snprintf(b, sizeof(buf)-(b-buf+strlen(e)+1+2), "%s", e);
      b+=strlen(b)+1;
    }      
    e+=strlen(e)+1;
  }
  FreeEnvironmentStrings(envs);
  
  hP = CreateFile(pipename,
		  GENERIC_READ|GENERIC_WRITE,
                  0,
                  NULL,
                  OPEN_EXISTING,
                  0,
                  NULL);
  if(hP == INVALID_HANDLE_VALUE){
    snprintf(msg, sizeof(msg)-1, "Invalid handle value : named pipe [%s]", pipename);
    MessageBox(NULL,
	       msg,
	       "Rhpc:fakemaster",
	       MB_OK);
    return 1;
  }
  
  if(0==WriteFile(hP,
		  buf,
		  sizeof(buf),
		  &dwNumberOfBytesWritten, NULL)){
    snprintf(msg, sizeof(msg)-1, "Faild to write named pipe [%s]", pipename);
    MessageBox(NULL,
	       msg,
	       "Rhpc:fakemaster",
	       MB_OK);
    CloseHandle(hP);
    return(-1);
  }
  FlushFileBuffers(hP);

  if(0==ReadFile(hP,
		  buf,
		  sizeof(buf),
		  &dwNumberOfBytesRead, NULL)){
    snprintf(msg, sizeof(msg)-1, "Can't detected Rhpc_finalize() on fakemaster[pipe:%s].", pipename);
    MessageBox(NULL,
	       msg,
	       "Rhpc:fakemaster",
	       MB_OK);
    CloseHandle(hP);
    return(-1);
  }

  CloseHandle(hP);
  return 0;
}
