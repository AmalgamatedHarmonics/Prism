#include "Droplet.hpp"

using namespace droplet;

void Audio::ChannelProcess(droplet::IO &io, rack::engine::Input &input, rack::engine::Output &output, droplet::Filter &filter) {

	dsp::Frame<1> iFrame = {};
	dsp::Frame<1> oFrame = {};

	float sample = 0.0f;
	int inLen1 = 0;
	int outLen1 = 0;
	int inLen2 = 0;
	int outLen2 = 0;

	if (!input.isConnected()) {
		float nO;
		switch (noiseSelected) {
			case 0:
				nO = brown.next() * 10.0f - 5.0f;
				break;
			case 1:
				nO = pink.next() * 10.0f - 5.0f;
				break;
			case 2:
				nO = white.next() * 10.0f - 5.0f;
				break;
			default:
				nO = pink.next() * 10.0f - 5.0f;
		}

		sample = nO / 5.0f;
	} else {
		sample = input.getVoltage(0) / 5.0f;
	}

	if (!inputBuffer.full()) {
		iFrame.samples[0] = sample;
		inputBuffer.push(iFrame);
	}

	// Process buffer
	if (outputBuffer.empty()) {

		inputSrc.setRates(sampleRate, SAMPLE_RATE);
		dsp::Frame<1> iFrames[NUM_SAMPLES] = {};

		inLen1 = inputBuffer.size();
		outLen1 = NUM_SAMPLES;
		inputSrc.process(inputBuffer.startData(), &inLen1, iFrames, &outLen1);
		inputBuffer.startIncr(inLen1);

		for (int j = 0; j < NUM_SAMPLES; j++) {
			io.in[j] = (int32_t)clamp(iFrames[j].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
		}

		// Pass to filter
		filter.filter();

		dsp::Frame<1> oFrames[NUM_SAMPLES] = {};
		for (int i = 0; i < NUM_SAMPLES; i++) {
			oFrames[i].samples[0] += io.out[i] / MAX_12BIT;
		}

		outputSrc.setRates(SAMPLE_RATE, sampleRate);
		inLen2 = NUM_SAMPLES;
		outLen2 = outputBuffer.capacity();
		outputSrc.process(oFrames, &inLen2, outputBuffer.endData(), &outLen2);
		outputBuffer.endIncr(outLen2);
	}

	// Set output
	if (!outputBuffer.empty()) {
		oFrame = outputBuffer.shift();
		output.setChannels(1);
		output.setVoltage(oFrame.samples[0] * 5.0f * SAMPLE_C);
	}

}

