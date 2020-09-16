#include "MpSeries.hpp"
#include <string.hpp>
#include <patch.hpp>

struct MpMinilab : Module {
	
    //ClockDivider to process only every N samples for optimisation.
    dsp::ClockDivider knobUpdateDivider;

	// The textField[] array holds pointers to the TextField widgets
	// created in the widget constructor.
	static const int NUM_TEXTFIELDS=13;        // 4 for each bank of 4 knobs, + 8 for each pad, + 1 for notes
	TextField *textField[NUM_TEXTFIELDS];

    static const int NUM_PARAMS  = 16;
	static const int NUM_INPUTS  =  1;
	static const int NUM_OUTPUTS = 16;
	static const int NUM_LIGHTS  =  0;
    
    void process(const ProcessArgs &args)override;
	
	MpMinilab() {		
		config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
		for (int i=0;i<NUM_PARAMS;i++) {
			configParam(i, 0, 10, 0);
		}
		knobUpdateDivider.setDivision(512);
	}
	
	//Make the name of the text field to be stored in json.
	//Because the json functions take const char* as the name we have 
	//sprintf the name to a char[] and return a pointer to that.
	//
	//Make sure the input char array name[] is long enough to hold the generated name.
	//
	//Format is "text<n>" e.g. "text12"
	//
	//So the json comes out like:
	//"data": {
    //	"text0": "OSC1    ATT     DEPTH     RES       DRV",
    //  "text1": "OSC2    ATT     F2 Frq    SyncFrq   OSC3 Vol",
	//...
	//
	const char* getJsonTextFieldName(char name[], int i) { 
		sprintf(name,"text%i",i);
		const char* nameptr = name;
		return nameptr;
	}
	
	//Save and Load state from json. 
	//Unlike the knobs (inputs) the text box values don't
	//get saved automatically.
	json_t* dataToJson() override {	
		json_t* rootJ = json_object();
		char name[12];
		for (int i=0;i<NUM_TEXTFIELDS;i++) {
			json_object_set_new(rootJ, getJsonTextFieldName(name,i), json_string(textField[i]->text.c_str()));
		}
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {	
		json_t* textJ;
		char name[12];
		for (int i=0;i<NUM_TEXTFIELDS;i++) {
			textJ = json_object_get(rootJ, getJsonTextFieldName(name,i));
			if (textJ) textField[i]->text = json_string_value(textJ);
		}
	}

};


void MpMinilab::process(const ProcessArgs &args) {
	if (knobUpdateDivider.process()) {
		//If the input is connected then...
		//Get the CV of each channel of the one polyphonic input
		//Set all 16 knobs to reflect the voltages of each channel
		//Send each CV to the 16 outputs
		//
		//If the input is not connected, just use
		//as a normal controller.
		float v;
		if (inputs[0].isConnected()) {
			for (int i=0;i<NUM_OUTPUTS;i++) {
				v = inputs[0].getVoltage(i);
				paramQuantities[i]->setValue(v);
				outputs[i].setVoltage(v);
			}
		} else {
			for (int i=0;i<NUM_OUTPUTS;i++) {
				v = paramQuantities[i]->getValue();
				outputs[i].setVoltage(v);
			}
		}
	}
};



struct MpMinilabWidget : ModuleWidget { 
   MpMinilabWidget(MpMinilab *module);
};


MpMinilabWidget::MpMinilabWidget(MpMinilab *module) {
	setModule(module);

	box.size = Vec(600, 380);

	//Background
	SvgPanel *panel = new SvgPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MinilabBackground.svg")));
	addChild(panel);

    //Knobs
	//Arranged in 4 banks of 4
    int x,y;
    int dx = 62;  // spacing between elements	
	int knobSetLeft[4] = {60, 350, 60,350};  
	int knobSetTop[4]  = {130,130,230,230};
	
	int id=0;
	for (int i=0;i<4;i++) {
		x = knobSetLeft[i];
		y = knobSetTop[i];
		for (int j=0;j<4;j++) {
			addParam(createParam<soxsaKnob1>(Vec(x+dx*j,y),module,id));
			id++;
		}
	}
	
	//Poly input jack.
	id = 0;
    addInput(createInput<jack>(Vec(50,47), module, id));
    
    //Output Jacks
	int offset = 30;  //Offset each jack diagonally down and right from its knob.
	id = 0;
	for (int i=0;i<4;i++) {
		x = knobSetLeft[i] + offset;
		y = knobSetTop[i]  + offset;
		for (int j=0;j<4;j++) {
			addOutput(createOutput<jackOutput>(Vec(x+dx*j,y), module, id));
			id++;
		}
	}
    

	
    //Text Fields
	float text_left[MpMinilab::NUM_TEXTFIELDS]  = {5, 300,  5,300};
	float text_top[MpMinilab::NUM_TEXTFIELDS]   = {90, 90,190,190};
	
	TextField* tf;
    for (int i=0;i<4;i++) {
		tf = createWidget<LedDisplayTextField>(Vec(text_left[i],text_top[i]));
		tf->box.size = mm2px(Vec(99, 12));
		tf->multiline = false;
		addChild(tf);
		
		//If the module ref exists, then keep a ref with the module
		//so we can save the data to json.
		//Note that the module ref does not exist when this function
		//is called from the module browser.
		if (module) {
			module->textField[i] = tf;
		}
	}
	
	//Bank of eight text fields to show what the Minilab pads
	//are setup to do.
	x = 22;
	y = 300;
	for (int i=0;i<8;i++) {
		tf = createWidget<LedDisplayTextField>(Vec(x,y));
		tf->box.size = mm2px(Vec(22, 20));
		tf->multiline = true;
		addChild(tf);
		x += 70;
		if (module) {
			module->textField[i+4] = tf;
		}
	}
	
	//Text field notes at the top.
	tf = createWidget<LedDisplayTextField>(Vec(170, 6));
	tf->box.size = mm2px(Vec(143, 20));
	tf->multiline = true;
	addChild(tf);
	if (module) {
		module->textField[12] = tf;
	}
	

}
Model *modelMpMinilab = createModel<MpMinilab,MpMinilabWidget>("MpMinilab");
