#pragma once

#include "ofMain.h"
#include <mutex>
#include <atomic>

class ofApp : public ofBaseApp{

	public:

		void setup();
		void update();
		void draw();

		void keyPressed  (int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
		vector<float> spectrum;
		float dominantFrequency = 0.0f;
		void computeDFT();
		
		void audioOut(ofSoundBuffer & buffer);
		
		
		ofSoundStream soundStream;

		float 	pan;
		int		sampleRate;
		bool 	bNoise;
		float 	volume;

		vector <float> lAudio;
		vector <float> rAudio;
		
		//------------------- for the simple sine wave synthesis
		float 	targetFrequency;
		float 	phase;
		float 	phaseAdder;
		float 	phaseAdderTarget;

		// waveform selection and additive synthesis control
		int formeOnde = 0; // 0=sine, 1=square, 2=saw
		int brillance = 10;

		// harmony controls
		vector<float> harmonics;
		string harmonyName = "None";

		// mutex to protect harmonics across audio/main threads
		std::mutex harmonicsMutex;

		// per-partial phase accumulators for non-integer ratios
		vector<float> partialPhases;

		// sample counter to compute real time in seconds for synthesis
		std::atomic<uint64_t> sampleIndex{0};

		float calcul_carre(float A, float f, float t, int brillance);
		float calcul_scie(float A, float f, float t, int brillance);
};
