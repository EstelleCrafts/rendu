#include "ofApp.h"

//--------------------------------------------------------------
//--- Version 2 --- 
void ofApp::setup(){

	ofBackground(34, 34, 34);
	
	int bufferSize		= 512;
	sampleRate 			= 44100;
	phase 				= 0;
	phaseAdder 			= 0.0f;
	phaseAdderTarget 		= 0.0f;
	volume				= 0.1f;
	bNoise 				= false;

	lAudio.assign(bufferSize, 0.0);
	rAudio.assign(bufferSize, 0.0);
	spectrum.assign(bufferSize / 2, 0.0f);
	dominantFrequency = 0.0f;
	
	soundStream.printDeviceList();

	ofSoundStreamSettings settings;

	// Je sélectionne le périphérique de sortie par défaut (sinon ça peut être un bordel à configurer pour l'utilisateur)
	auto devices = soundStream.getMatchingDevices("default");
	if(!devices.empty()){
		settings.setOutDevice(devices[0]);
	}

	settings.setOutListener(this);
	settings.sampleRate = sampleRate;
	settings.numOutputChannels = 2;
	settings.numInputChannels = 0;
	settings.bufferSize = bufferSize;
	soundStream.setup(settings);


	// J'initialise les harmoniques avec une seule fondamentale pour éviter les problèmes de synthèse à l'ouverture (si on laisse vide ça peut causer des NaN dans le calcul de la sinusoïde)
	harmonics = {1.0f};
	harmonyName = "Fundamental";
}

//--------------------------------------------------------------
void ofApp::update(){
	computeDFT();
}

//--------------------------------------------------------------
void ofApp::draw(){

	ofSetColor(225);
	ofDrawBitmapString("AUDIO OUTPUT EXAMPLE", 32, 32);
	ofDrawBitmapString("press 's' to unpause the audio\npress 'p' to pause the audio", 31, 92);
	
	ofNoFill();
	
	// Je dessine le canal gauche :
	ofPushStyle();
		ofPushMatrix();
		ofTranslate(32, 150, 0);
			
		ofSetColor(225);
		ofDrawBitmapString("Left Channel", 4, 18);
		
		ofSetLineWidth(1); 
		ofDrawRectangle(0, 0, 900, 200);

		ofSetColor(245, 58, 135);
		ofSetLineWidth(3);
				
			ofBeginShape();
			for (unsigned int i = 0; i < lAudio.size(); i++){
				float x =  ofMap(i, 0, lAudio.size(), 0, 900, true);
				ofVertex(x, 100 -lAudio[i]*180.0f);
			}
			ofEndShape(false);
			
		ofPopMatrix();
	ofPopStyle();

	// Je dessine le canal droit :
	ofPushStyle();
		ofPushMatrix();
		ofTranslate(32, 350, 0);
			
		ofSetColor(225);
		ofDrawBitmapString("Right Channel", 4, 18);
		
		ofSetLineWidth(1); 
		ofDrawRectangle(0, 0, 900, 200);

		ofSetColor(245, 58, 135);
		ofSetLineWidth(3);
				
			ofBeginShape();
			for (unsigned int i = 0; i < rAudio.size(); i++){
				float x =  ofMap(i, 0, rAudio.size(), 0, 900, true);
				ofVertex(x, 100 -rAudio[i]*180.0f);
			}
			ofEndShape(false);
			
		ofPopMatrix();
	ofPopStyle();

	// Je dessine le spectre :
	ofPushStyle();
		ofPushMatrix();
		ofTranslate(500, 40, 0);

		ofSetColor(225);
		ofDrawBitmapString("Spectrum (Left Channel)", 4, 18);

		ofSetLineWidth(1);
		ofDrawRectangle(0, 0, 420, 140);

		ofSetColor(58, 190, 245);
		ofSetLineWidth(2);

		ofBeginShape();
		for (unsigned int i = 0; i < spectrum.size(); i++){
			float x = ofMap(i, 0, spectrum.size(), 0, 420, true);
			float y = ofMap(spectrum[i], 0.0f, 0.25f, 140, 0, true); 
			ofVertex(x, y);
		}
		ofEndShape(false);

		ofSetColor(225);
		ofDrawBitmapString("Dominant freq: " + ofToString(dominantFrequency, 1) + " Hz", 4, 158);
		ofPopMatrix();
	ofPopStyle();
		
	ofSetColor(225);
	// Affichage clair de la forme d'onde et de la brillance
	string reportString = "volume: ("+ofToString(volume, 2)+") modify with -/+ keys\n";
	reportString += "pan: ("+ofToString(pan, 2)+") modify with mouse x\n";
	reportString += "Forme d'onde: ";
	switch(formeOnde){
		case 0: reportString += "Sinusoïde (1)"; break;
		case 1: reportString += "Carré (2)"; break;
		case 2: reportString += "Dent de scie (3)"; break;
		default: reportString += "Sinusoïde"; break;
	}
	int additiveAvailable = 0;
	{
		std::lock_guard<std::mutex> lock(harmonicsMutex);
		const int maxHarmonics = 16;
		if(targetFrequency > 0.0f){
			float nyquist = (float)sampleRate * 0.5f;
			for(float h : harmonics){
				if(h * targetFrequency <= nyquist){
					++additiveAvailable;
					if(additiveAvailable >= maxHarmonics) break;
				}
			}
		}else{
			additiveAvailable = std::min((int)harmonics.size(), maxHarmonics);
		}
	}

	int localBrillance = std::max(1, brillance);
	if(formeOnde == 0){
		int active = std::min(localBrillance, additiveAvailable);
		reportString += "\n nb harmonie active " + ofToString(localBrillance) +
						" -> " + ofToString(active) + " harmoniques actives (UP/DOWN)\n";
	}else{
		reportString += "\nBrillance (Fourier): " + ofToString(localBrillance) +
						" termes (UP/DOWN)\n";
	}

	// Affichage de l'harmonie (copie protégée par mutex)
	string harmStr = "[";
	string localHarmonyName;
	{
		std::lock_guard<std::mutex> lock(harmonicsMutex);
		for(size_t hi=0; hi<harmonics.size(); ++hi){
			harmStr += ofToString(harmonics[hi], 2);
			if(hi+1 < harmonics.size()) harmStr += ", ";
		}
		localHarmonyName = harmonyName;
	}
	harmStr += "]";
	reportString += "Harmony: " + localHarmonyName + " " + harmStr + "\n";

	reportString += "dominant frequency: " + ofToString(dominantFrequency, 1) + " Hz";
	ofDrawBitmapString(reportString, 32, 579);

}

//--------------------------------------------------------------
void ofApp::keyPressed  (int key){
	if (key == '-' || key == '_' ){
		volume -= 0.05;
		volume = std::max(volume, 0.f);
	} else if (key == '+' || key == '=' ){
		volume += 0.05;
		volume = std::min(volume, 1.f);
	}
	
	if( key == 's' ){
		soundStream.start();
	}
	
	if( key == 'p' ){
		soundStream.stop();
	}

	// sélection de la forme d'onde: 1=sine, 2=square, 3=saw
	if(key == '1'){
		formeOnde = 0; // sinus
	} else if(key == '2'){
		formeOnde = 1; // carré
	} else if(key == '3'){
		formeOnde = 2; // dent de scie
	}

	// brillance up/down, limitée entre 1 et 50
	if(key == OF_KEY_UP){
		brillance = std::min(brillance + 1, 50);
	}
	if(key == OF_KEY_DOWN){
		brillance = std::max(brillance - 1, 1);
	}

	// Raccourcis d'harmonie : a z e r t y u (présets)
	auto sanitize = [&](const std::vector<float>& src){
		const int MAX_H = 32;
		std::vector<float> dst;
		dst.reserve(std::min((size_t)MAX_H, src.size()));
		for(float v : src){
			if(!std::isfinite(v)) continue;
			if(v <= 0.0f) continue;
			float clamped = std::min(v, 128.0f);
			dst.push_back(clamped);
			if((int)dst.size() >= MAX_H) break;
		}
		if(dst.empty()) dst.push_back(1.0f);
		return dst;
	};

	if(key == 'a'){
		auto tmp = sanitize({1.0f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "Fundamental";
		}
		ofLogNotice("HARM") << "Set Fundamental => size=" << harmonics.size();
	}
	if(key == 'z'){
		auto tmp = sanitize({1.0f, 2.0f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "Octave";
		}
		ofLogNotice("HARM") << "Set Octave => size=" << harmonics.size();
	}
	if(key == 'e'){
		auto tmp = sanitize({1.0f, 1.25f, 1.5f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "Major";
		}
		ofLogNotice("HARM") << "Set Major => size=" << harmonics.size();
	}
	if(key == 'r'){
		auto tmp = sanitize({1.0f, 1.2f, 1.5f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "Minor";
		}
		ofLogNotice("HARM") << "Set Minor => size=" << harmonics.size();
	}
	if(key == 't'){
		auto tmp = sanitize({1.0f, 1.5f, 2.0f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "PowerChord";
		}
		ofLogNotice("HARM") << "Set PowerChord => size=" << harmonics.size();
	}
	if(key == 'y'){
		auto tmp = sanitize({1.0f,2.0f,3.0f,4.0f,5.0f,6.0f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "Rich";
		}
		ofLogNotice("HARM") << "Set Rich => size=" << harmonics.size();
	}
	if(key == 'u'){
		auto tmp = sanitize({1.0f,2.0f,3.0f,4.0f,5.0f,6.0f,7.0f,8.0f,9.0f,10.0f});
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			harmonics = tmp;
			harmonyName = "VeryRich";
		}
		ofLogNotice("HARM") << "Set VeryRich => size=" << harmonics.size();
	}

}

//--------------------------------------------------------------
void ofApp::keyReleased  (int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
	int width = ofGetWidth();
	pan = (float)x / (float)width;
	float height = (float)ofGetHeight();
	float heightPct = ((height-y) / height);
	targetFrequency = 2000.0f * heightPct;
	phaseAdderTarget = (targetFrequency / (float) sampleRate) * glm::two_pi<float>();
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
	int width = ofGetWidth();
	pan = (float)x / (float)width;
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	bNoise = true;
}


//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
	bNoise = false;
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
float ofApp::calcul_carre(float A, float f, float t, int brillance){
	if(f <= 0.0f) return 0.0f;
	float base = glm::two_pi<float>() * f * t; // 2π f t
	float sum = 0.0f;
	int B = std::max(1, brillance);
	for(int k = 0; k < B; k++){
		int n = 2 * k + 1;
		sum += sin(n * base) / (float)n;
	}
	return (4.0f * A / glm::pi<float>()) * sum;
}

//--------------------------------------------------------------
float ofApp::calcul_scie(float A, float f, float t, int brillance){
	if(f <= 0.0f) return 0.0f;
	float base = glm::two_pi<float>() * f * t; // 2π f t
	float sum = 0.0f;
	int B = std::max(1, brillance);
	for(int k = 1; k <= B; k++){
		float sign = (k % 2 == 0) ? 1.0f : -1.0f; // (-1)^k
		sum += sign * sin(k * base) / (float)k;
	}
	return (2.0f * A / glm::pi<float>()) * sum;
}

//--------------------------------------------------------------
void ofApp::audioOut(ofSoundBuffer & buffer){
	// pan = 0.5f;
	float leftScale = 1 - pan;
	float rightScale = pan;

	// La fonction sin(n) déconne si n devient très grand, du coup
	// je limite la phase dans l'intervalle 0 - glm::two_pi<float>().
	while (phase > glm::two_pi<float>()){
		phase -= glm::two_pi<float>();
	}

	// Je copie les harmoniques une fois par buffer pour éviter des locks à chaque échantillon
	std::vector<float> localHarmonics;
	std::vector<float> localPartialPhases;
	{
		std::lock_guard<std::mutex> lock(harmonicsMutex);
		localHarmonics = harmonics;
		// Je copie les phases partielles si elles existent et font la même taille, sinon je les initierai après
		localPartialPhases = partialPhases;
	}

	// Je filtre les harmoniques pour éviter l'aliasing (j'enlève celles au-dessus de Nyquist)
	const int MAX_HARMONICS = 16;
	std::vector<float> filteredHarmonics;
	if(targetFrequency > 0.0f && !localHarmonics.empty()){
		float nyquist = (float)sampleRate * 0.5f;
		for(float h : localHarmonics){
			if((float)h * targetFrequency <= nyquist){
				filteredHarmonics.push_back(h);
				if((int)filteredHarmonics.size() >= MAX_HARMONICS) break;
			}
		}
	} else {
		// Si on n'a pas encore de fréquence cible, j'utilise localHarmonics (avec une limite)
		for(size_t i=0; i<localHarmonics.size() && (int)filteredHarmonics.size() < MAX_HARMONICS; ++i)
			filteredHarmonics.push_back(localHarmonics[i]);
	}

	// Je m'assure que localPartialPhases a la même taille que filteredHarmonics
	if(localPartialPhases.size() != filteredHarmonics.size()){
		localPartialPhases.assign(filteredHarmonics.size(), 0.0f);
	}

	int localBrillance = std::max(1, brillance);

	if ( bNoise == true){
		// ---------------------- bruit --------------
		for (size_t i = 0; i < buffer.getNumFrames(); i++){
			lAudio[i] = buffer[i*buffer.getNumChannels()    ] = ofRandom(0, 1) * volume * leftScale;
			rAudio[i] = buffer[i*buffer.getNumChannels() + 1] = ofRandom(0, 1) * volume * rightScale;
		}
	} else {
		phaseAdder = 0.95f * phaseAdder + 0.05f * phaseAdderTarget;
		for (size_t i = 0; i < buffer.getNumFrames(); i++){
			phase += phaseAdder;

			float sample = 0.0f;

			if(formeOnde == 0){
				// Onde sinusoïdale additive : j'additionne les partiels selon les multiplicateurs
				if(filteredHarmonics.empty()){
					// Sinon je retombe sur une simple sinusoïde
					sample = sin(phase);
				} else {
					float sum = 0.0f;
					int activePartials = std::min((int)filteredHarmonics.size(), localBrillance);
					// Je calcule l'incrément de phase pour chaque partiel et j'accumule les phases
					for(int h = 0; h < activePartials; ++h){
						float ratio = filteredHarmonics[h];
						float freq = targetFrequency * ratio;
						// Incrément de phase en radians par échantillon
						float inc = (freq / (float)sampleRate) * glm::two_pi<float>();
						localPartialPhases[h] += inc;
						if(localPartialPhases[h] > glm::two_pi<float>()) localPartialPhases[h] -= glm::two_pi<float>();
						sum += sin(localPartialPhases[h]);
					}
					sample = (activePartials > 0) ? (sum / (float)activePartials) : 0.0f;
				}
			} else if(formeOnde == 1){
				// square via additive synthesis (use real time t in seconds)
				if(targetFrequency > 0.0f){
					float t = (float)sampleIndex.load(std::memory_order_relaxed) / (float)sampleRate;
					sample = calcul_carre(1.0f, targetFrequency, t, localBrillance);
				}else{
					sample = 0.0f;
				}
			} else if(formeOnde == 2){
				// sawtooth via additive synthesis (use real time t in seconds)
				if(targetFrequency > 0.0f){
					float t = (float)sampleIndex.load(std::memory_order_relaxed) / (float)sampleRate;
					sample = calcul_scie(1.0f, targetFrequency, t, localBrillance);
				}else{
					sample = 0.0f;
				}
			}

			lAudio[i] = buffer[i*buffer.getNumChannels()    ] = sample * volume * leftScale;
			rAudio[i] = buffer[i*buffer.getNumChannels() + 1] = sample * volume * rightScale;

			// J'incrémente le compteur d'échantillons (utile pour calculer t en secondes)
			sampleIndex.fetch_add(1, std::memory_order_relaxed);
		}
	}

		// J'écris les phases partielles en mémoire partagée sous verrou
		{
			std::lock_guard<std::mutex> lock(harmonicsMutex);
			partialPhases = localPartialPhases;
		}
}

//--------------------------------------------------------------
void ofApp::computeDFT(){
	//nombre d'echantillons
	int N = (int)lAudio.size();
	// Si rien on arrete
	if (N == 0) return;

	// nb echantillons utiles (miroir)
	int K = N / 2;

	// spectrum c'est un tableau qui doit faire la taille de K, on le remplit de 0 si ce n'est pas le cas
	// chaque case k contiendra la magnitude du signal pour la frequence k*sampleRate/N
	if ((int)spectrum.size() != K){
		spectrum.assign(K, 0.0f);
	}

	float maxMag = 0.0f;
	int peakBin = 0;
	float df = (float)sampleRate / (float)N;

	// 1) On remplit le spectre pour chaque frequence k
	for (int k = 0; k < K; k++){
		float real = 0.0f;
		float imag = 0.0f;

		for (int n = 0; n < N; n++){
			float phase = glm::two_pi<float>() * (float)k * (float)n / (float)N;
			real += lAudio[n] * cos(phase);
			imag -= lAudio[n] * sin(phase);
		}

		// calcul de la magnitude du signal pour la frequence k*sampleRate/N
		float mag = sqrt(real * real + imag * imag) / (float)N;
		spectrum[k] = mag;

		// 2) On enregistre la mag la plus haute
		// j'ignore la case DC pour la fréquence dominante
		if (k > 0 && mag > maxMag){
			maxMag = mag;
			peakBin = k;
		}
	}

	// 3) On calcule la frequence dominante
	// Converti en frequence en multipliant par sampleRate/N
	dominantFrequency = peakBin * df;
}
//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

