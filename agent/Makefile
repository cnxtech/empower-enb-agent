# Copyright (c) 2019 FBK
# Designed by Roberto Riggio
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the
# specific language governing permissions and limitations
# under the License.

CC=gcc

AGENTP=.

LIBS=-lpthread -lemproto
INCLUDES=-I../include

all:
	$(CC) $(INCLUDES) -c -fpic -O2 -fvisibility=hidden $(AGENTP)/*.c
	$(CC) -shared -fpic -s -o libemagent.so *.o $(LIBS)

clean:
	rm -f ./*.o
	rm -f ./*.a
	rm -f ./*.so

debug:
	$(CC) $(INCLUDES) -c -fpic -DEBUG -fvisibility=hidden $(AGENTP)/*.c
	$(CC) -shared -fpic -s -o libemagent.so *.o $(LIBS)

install:
	cp ./libemagent.so $(INSTDIR)
	mkdir -p $(INCLDIR)
	cp -r ../include/emage.h $(INCLDIR)/

uninstall:
	rm $(INSTDIR)/libemagent.so
	rm -f $(INCLDIR)/emage.h
