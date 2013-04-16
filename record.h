#ifndef RECORD_H
#define RECORD_H

typedef void (*recordCallback) (short* buffer, int bufferSize);
void startRecording(recordCallback cb);

#endif // RECORD_H
