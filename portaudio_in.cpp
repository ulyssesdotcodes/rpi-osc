/** @file pa_fuzz.c
	@ingroup examples_src
    @brief Distort input like a fuzz box.
	@author Phil Burk  http://www.softsynth.com
*/
/*
 * $Id$
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

#include <iostream>
#include <stdio.h>
#include <math.h>
#include <portaudio.h>
#include <wiringPi.h>
#include <cmath>
#include <thread>

#include <oscpack/osc/OscReceivedElements.h>
#include <oscpack/osc/OscPacketListener.h>
#include <oscpack/ip/UdpSocket.h>

/*
** Note that many of the older ISA sound cards on PCs do NOT support
** full duplex audio (simultaneous record and playback).
** And some only support full duplex at lower sample rates.
*/
#define SAMPLE_RATE         (44100)
#define PA_SAMPLE_TYPE      paFloat32
#define FRAMES_PER_BUFFER   (64)
#define PWM_PIN (18)
#define PORT 3334

typedef float SAMPLE;

struct RpiState {
  int program;
  float level;
};

float CubicAmplifier( float input );

/* Non-linear amplifier with soft distortion curve. */
float CubicAmplifier( float input )
{
    float output, temp;
    if( input < 0.0 )
    {
        temp = input + 1.0f;
        output = (temp * temp * temp) - 1.0f;
    }
    else
    {
        temp = input - 1.0f;
        output = (temp * temp * temp) + 1.0f;
    }

    return output;
}
#define FUZZ(x) CubicAmplifier(CubicAmplifier(CubicAmplifier(CubicAmplifier(x))))

static float prevSum = 0.0;

void error(int err) {
  Pa_Terminate();
  fprintf( stderr, "An error occured while using the portaudio stream\n" );
  fprintf( stderr, "Error number: %d\n", err );
  fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
}

class RpiPacketListener : public osc::OscPacketListener {
public:
  RpiPacketListener() {
    mState = { 0, 0.2 };
  }

  virtual RpiState getState() {
    return mState;
  }

protected:

    virtual void ProcessMessage( const osc::ReceivedMessage& m, 
				const IpEndpointName& remoteEndpoint )
    {
        (void) remoteEndpoint; // suppress unused parameter warning

        std::cout << "received message" << std::endl;

        try{
            if( std::strcmp( m.AddressPattern(), "/rpi/program" ) == 0 ){
                osc::ReceivedMessageArgumentStream args = m.ArgumentStream();
                const char *a1;
                args >> a1 >> osc::EndMessage;

                std::string a1Str(a1);

                std::cout << "received '/rpi/program' message with arguments: "
                          << a1Str << std::endl;

                int prog = a1Str == "audioReactive" ? 1 : 0;

                mState = { prog, mState.level };
            }else if( std::strcmp( m.AddressPattern(), "/rpi/lightLevel" ) == 0 ){
              osc::ReceivedMessageArgumentStream args = m.ArgumentStream();
              float a1;
              args >> a1 >> osc::EndMessage;

              std::cout << "received '/rpi/lightLevel' message with arguments: "
                        << a1 << std::endl;

              mState = { mState.program, a1 };
            }
        }catch( osc::Exception& e ){
            // any parsing errors such as unexpected argument types, or 
            // missing arguments get thrown as exceptions.
            std::cout << "error while parsing message: "
                << m.AddressPattern() << ": " << e.what() << "\n";
        }
    }

private:
  RpiState mState;
};


/*******************************************************************/

    static RpiPacketListener listener;
    static int gNumNoInputs = 0;
    static float mPrevSum;
  static int fuzzCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
  {
      SAMPLE *out = (SAMPLE*)outputBuffer;
      const SAMPLE *in = (const SAMPLE*)inputBuffer;
      unsigned int i;
      (void) timeInfo; /* Prevent unused variable warnings. */
      (void) statusFlags;
      (void) userData;

      RpiState rpiState = listener.getState();

      float sum = 0;

      if (rpiState.program == 0) {
        sum = 1.0;
      }
      else if( inputBuffer == NULL )
      {
          for( i=0; i<framesPerBuffer; i++ )
          {
              *out++ = 0;  /* left - silent */
              *out++ = 0;  /* right - silent */
          }
          gNumNoInputs += 1;
      }
      else
      {
          for( i=0; i<framesPerBuffer; i++ )
          {
            sum += *in++;
          }
      }

      if (sum < mPrevSum) {
        sum = mPrevSum - 0.03;
      }

      mPrevSum = sum;

      int pwmVal = (int) std::abs(sum * rpiState.level * 1024);

      if ( pwmVal >= 0 ) {
        pwmWrite(PWM_PIN, pwmVal);
      }

      sum /= (float) framesPerBuffer;
      sum *= 4.0;

      return paContinue;
  }

void startRpi() {
  UdpListeningReceiveSocket s(IpEndpointName( IpEndpointName::ANY_ADDRESS, PORT ), &listener );
  s.Run();
}

int main(void);
int main(void)
{
  // WiringPi
  wiringPiSetupGpio();
  pinMode(PWM_PIN, PWM_OUTPUT);

  std::thread rpiThread(startRpi);

  //PA
    PaStreamParameters inputParameters;
    PaStream *stream;
    PaError err;

    err = Pa_Initialize();
    if( err != paNoError ) error(err);

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default input device.\n");
      error(err);
    }
    inputParameters.channelCount = 2;       /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;


    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              0, /* paClipOff, */  /* we won't output out of range samples so don't bother clipping them */
              fuzzCallback,
              NULL );

    if( err != paNoError ) error(err);

    err = Pa_StartStream( stream );
    if( err != paNoError ) error(err);

    printf("Hit ENTER to stop program.\n");
    getchar();
    err = Pa_CloseStream( stream );
    if( err != paNoError ) error(err);

    printf("Finished. gNumNoInputs = %d\n", gNumNoInputs );
    Pa_Terminate();

    rpiThread.join();


    return 0;

}
