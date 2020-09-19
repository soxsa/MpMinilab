#include "MpSeries.hpp"
#include <string.hpp>
#include <patch.hpp>

// VoltageDisplayWidget
// Graphic display of each voltage:
// 1. A column of lights with a range of colors
// 2. A small text display of the exact voltage
// 3. A large number showing the integer voltage (disabled)
struct VoltageDisplayWidget : Widget {
	
	float voltage;
	LightWidget *lights[11], *light;
	NVGcolor lightColors[11];
	
	VoltageDisplayWidget() {
		
	    // Init the colors for the lights
		NVGcolor color[3];
		color[0] = nvgRGB(0x00, 0xff, 0x00); //Green
		color[1] = nvgRGB(0xff, 0xff, 0x00); //Amber
		color[2] = nvgRGB(0xff, 0x00, 0x00); //Red
		
		// Create a vertical column of lights.
		for (int i=0;i<11;i++) {
			light = createWidget<LightWidget>(Vec(3,50-i*5));
			light->setSize(Vec(3,3));
			lights[i] = light;	
			addChild(light);
			
			//Set the color for each light.
			if (i<4) {
				lightColors[i] = color[0];
			} else if (i<7) {
				lightColors[i] = color[1];
			} else {
				lightColors[i] = color[2];
			}
		}
	}
	
    void draw(const DrawArgs &args) override {	
		char display_string[50];
	
	    /*
        //Set the color for the large text, depending on voltage.		
		NVGcolor textColor;
		if (voltage <=3) {
			textColor = nvgRGB(0x00, 0xff, 0x00);  // Lo
		}
		else if (voltage <=6) {
			textColor = nvgRGB(0x00, 0xff, 0xff);  // Med
		}
		else {
			textColor = nvgRGB(0xff, 0xff, 0xff);  // High
		}
		*/
		
		//Lights
		//NVGcolor lightOn  = nvgRGB(0xff,0xff,0xff);
		NVGcolor lightOff = nvgRGB(0x00,0x00,0x00);
		for (int i=0;i<11;i++) {
			if (voltage <= 0.0f) {
			    lights[i]->color = lightOff;
			} else if (voltage >= i) {
				lights[i]->color = lightColors[i];
			} else {
				lights[i]->color = lightOff;
			}
		}
		
		//Integer volts
		/*
		sprintf(display_string,"%2.0f",voltage);
		nvgFontSize(args.vg, 24);
		nvgFillColor(args.vg, textColor);	
		nvgText(args.vg,0,18, display_string, NULL);
		*/
		
		//Small voltage display e.g. 5.3
		sprintf(display_string,"%2.1f",voltage);
		nvgFontSize(args.vg, 8);
		nvgText(args.vg,10,50, display_string, NULL);
		
		Widget::draw(args);
	}
};



struct MpMinilab : Module {
	
	static const int NUM_PARAMS  = 16;
	static const int NUM_INPUTS  =  1;
	static const int NUM_OUTPUTS = 16;
	static const int NUM_LIGHTS  =  0;
	
	//When the input is connected, either move the knobs to 
	//reflect the voltage or not.
	bool moveKnobs;
	
    //ClockDivider to process only every N samples for optimisation.
    dsp::ClockDivider knobUpdateDivider;

	// The textField[] array holds pointers to the TextField widgets
	// created in the widget constructor.
	static const int NUM_TEXTFIELDS=13;        // 4 for each bank of 4 knobs, + 8 for each pad, + 1 for notes
	TextField *textField[NUM_TEXTFIELDS];
	VoltageDisplayWidget *voltageDisplay[NUM_OUTPUTS];
    
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
		
		//Move knobs to show voltage.
		json_object_set_new(rootJ,  "moveKnobs", json_boolean(moveKnobs)); 
		
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {	
		json_t* textJ;
		char name[12];
		for (int i=0;i<NUM_TEXTFIELDS;i++) {
			textJ = json_object_get(rootJ, getJsonTextFieldName(name,i));
			if (textJ) textField[i]->text = json_string_value(textJ);
		}
		
		//Move knobs to show voltage.
		moveKnobs = json_boolean_value(json_object_get(rootJ, "moveKnobs")); 
	}

};


void MpMinilab::process(const ProcessArgs &args) {
	if (knobUpdateDivider.process()) {
		//If the input is connected then...
		//Get the CV of each channel of the one polyphonic input
		//Optionally set all 16 knobs to reflect the voltages of each channel
		//Send each CV to the 16 outputs
		//
		//If the input is not connected, just use
		//as a normal controller and get the voltages 
		//from the knobs.
		//
		float v = 0.0f;
		if (inputs[0].isConnected()) {
			//Polyphonic input is connected. Take the voltage from that.
			for (int i=0;i<NUM_OUTPUTS;i++) {
				v = inputs[0].getVoltage(i);
				if (voltageDisplay[i]) {
					voltageDisplay[i]->voltage = v;
					
					//Optionally rotate the knobs to reflect the volume.
					if (moveKnobs) {
						paramQuantities[i]->setValue(v);
					}
				}
				outputs[i].setVoltage(v);
			}
		} else {
			//Input is not connected, take the voltage from the knobs.
			for (int i=0;i<NUM_OUTPUTS;i++) {
				v = params[i].getValue();
				outputs[i].setVoltage(v);
				if (voltageDisplay[i]) {
					voltageDisplay[i]->voltage = v;
				}
			}
		}
	}
};



struct MpMinilabWidget : ModuleWidget { 
   MpMinilabWidget(MpMinilab *module);
   
   void appendContextMenu(Menu* menu) override {
		MpMinilab* module = dynamic_cast<MpMinilab*>(this->module);

		//Menu item to toggle between moving/not moving the knobs based on input voltage
		struct MoveKnobs : MenuItem {
			
			MpMinilab* module;
			
			void onAction(const event::Action& e) override {
				module->moveKnobs = !module->moveKnobs;
			}
			void step() override {
				rightText = (module->moveKnobs) ? "✔" : "";
				MenuItem::step();
			}
		};

		//Add the menu items.
		menu->addChild(new MenuSeparator());
     
		//moveKnobs menu item.
		MoveKnobs* moveKnobsMenuItem = createMenuItem<MoveKnobs>("Move Knobs to Show Voltage");
		moveKnobsMenuItem->module = module;
		menu->addChild(moveKnobsMenuItem);

	}
};


MpMinilabWidget::MpMinilabWidget(MpMinilab *module) {
	setModule(module);

	box.size = Vec(600, 380);

	//Background
	SvgPanel *panel = new SvgPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SoxsaMinilabBackground.svg")));
	addChild(panel);
	
	//Voltage Display
	VoltageDisplayWidget* vdw;
	int x,y;
    int dx = 62;  // spacing between elements	
    int offsetX = -15;  
	int offsetY = 0;
	int knobSetLeft[4] = {60, 350, 60,350};  
	int knobSetTop[4]  = {130,130,230,230};	
	int id=0;
	for (int i=0;i<4;i++) {
		x = knobSetLeft[i]+offsetX;
		y = knobSetTop[i]+offsetY;
		for (int j=0;j<4;j++) {
			vdw = createWidget<VoltageDisplayWidget>(Vec(x+dx*j,y));
			if (module) module->voltageDisplay[id] = vdw;
			addChild(vdw);
			id++;
		}
	}
	

    //Knobs
	//Arranged in 4 banks of 4
	id=0;
	offsetX = 8;  //Offset each jack diagonally down and right from its knob.
	offsetY = 50;
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
    addInput(createInput<jack>(Vec(10,316), module, id));
    
    //Output Jacks
	offsetX = 12;  //Offset each jack diagonally down and right from its knob.
	offsetY = 38;
	id = 0;
	for (int i=0;i<4;i++) {
		x = knobSetLeft[i] + offsetX;
		y = knobSetTop[i]  + offsetY;
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
	x = 38;
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
