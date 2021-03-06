/*
  ==============================================================================
  SbRadioGroup
  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

static const int radioGroupId = 99; // there is only one group in the component. Other radiogroups can use the same ID as long as they live in a different component.

template<typename ButtonType> 
class SbRadioGroupAttachment;

template<typename ButtonType> 
class SbRadioGroup : public Component, public ButtonListener
{
public:
	//==============================================================================
	SbRadioGroup(int itemWidth, int itemHeight = 24, bool isRow = false)
		: itemWidth(itemWidth), itemHeight(itemHeight), isRow(isRow), numButtons(0)
	{}
	virtual ~SbRadioGroup() {}
	void addButton(const String& name) {
		buttons.add(new ButtonType(name));
		addAndMakeVisible(buttons[numButtons]);
		buttons[numButtons]->setRadioGroupId(radioGroupId);
		buttons[numButtons]->setClickingTogglesState(true);
		buttons[numButtons]->addListener(this);
		++numButtons;
	}

	void clear() {
		numButtons = 0;
		buttons.clear();
	}

	class Listener {
	public:
		virtual void radioGroupStateChanged(SbRadioGroup* s, int state) = 0;
	};

	void buttonClicked(Button* buttonThatWasClicked) override {
		if (buttonThatWasClicked->getToggleState()) {
			jassert(dynamic_cast<ButtonType*>(buttonThatWasClicked));
			ButtonType* tb = static_cast<ButtonType*>(buttonThatWasClicked);
			state = buttons.indexOf(tb);
			listeners.call(&Listener::radioGroupStateChanged, this, state);
		}
	}

	void setState(int btnIdx, const NotificationType notification) {
		if (btnIdx >= 0)
		state = btnIdx >= 0 && btnIdx < numButtons ? btnIdx : defaultState;
		buttons[state]->setToggleState(true, dontSendNotification);
		if (notification != dontSendNotification)
			listeners.call(&Listener::radioGroupStateChanged, this, state);
	}

	void setState() {
		state = defaultState;
		buttons[state]->setToggleState(true, dontSendNotification);
	}
	int getState() {
		return state;
	}

	void paint(Graphics& g) override {}
	void resized() override {
		if (isRow) 
			for (int idx = 0; idx < numButtons; ++idx) {
				buttons[idx]->setBounds(idx * itemWidth, 0, itemWidth, itemHeight);
				buttons[idx]->setConnectedEdges(((idx != 0) ? Button::ConnectedOnLeft : 0)
					| ((idx != buttons.size()-1) ? Button::ConnectedOnRight : 0));
			}
		else
			for (int idx = 0; idx < numButtons; ++idx) {
				buttons[idx]->setBounds(0, idx * itemHeight, itemWidth, itemHeight);
				buttons[idx]->setConnectedEdges(((idx != 0) ? Button::ConnectedOnTop : 0)
					| ((idx != buttons.size()-1) ? Button::ConnectedOnBottom : 0));
			}
	}


	void addListener(Listener* lst) { listeners.add(lst); }
	void removeListener(Listener* lst) { listeners.remove(lst); }
	void setDefaultState(float newDefaultState){ defaultState = newDefaultState; }
protected:
	friend SbRadioGroupAttachment<ButtonType>;
	int numButtons;
	OwnedArray<ButtonType> buttons;
	ListenerList<Listener> listeners;
	int state, defaultState;
	int itemWidth, itemHeight;
	bool isRow;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SbRadioGroup)
};


template<typename ButtonType>
class JUCE_API  SbRadioGroupAttachment : private AttachedControlBase,
	private SbRadioGroup<ButtonType>::Listener
{
public:
	SbRadioGroupAttachment(AudioProcessorValueTreeState& s, const String& p, SbRadioGroup<ButtonType>& rg, bool populateGroup = true)
		: AttachedControlBase(s, p), rg(rg),  ignoreCallbacks(false)
	{
		range = new NormalisableRange<float>(s.getParameterRange(p));
		if (range->interval == 0.f) return;
		rangScale = 1.f / range->interval;
		if (populateGroup) {
			rg.clear();
			for (float btnVal = range->start; btnVal <= range->end; btnVal += range->interval) {
				rg.addButton(getParameter()->getText(range->convertTo0to1(btnVal), 64));
			}
		}
		else {
			/*
			It seems like the number of buttons available in the group is not equal to the number of choices provided by the parameter.
			*/
			jassert(ceilf((range->end - range->start) * rangScale) + 1 == rg.buttons.size());
		}
		rg.setDefaultState(getBtnIdx(getParameter()->defaultValue));
		rg.resized();
		sendInitialUpdate();
		rg.addListener(this);
	}

	~SbRadioGroupAttachment() {
		removeListener();
	}

	void removeListener() {
		AttachedControlBase::removeListener();
		rg.removeListener(this);
	}

	void setValue(float newValue) override {
		const ScopedLock selfCallbackLock(selfCallbackMutex);
		{
			ScopedValueSetter<bool> svs(ignoreCallbacks, true);
			rg.setState(getBtnIdx(newValue), sendNotificationSync);
		}
	}
	void radioGroupStateChanged(SbRadioGroup<ButtonType>* s, int newState) override {
		const ScopedLock selfCallbackLock(selfCallbackMutex);
		if ((!ignoreCallbacks) && (!ModifierKeys::getCurrentModifiers().isRightButtonDown()))
			setNewUnnormalisedValue(btnIdxToValue(newState));
	}
private:
	SbRadioGroup<ButtonType>& rg;
	ScopedPointer<NormalisableRange<float>>range;
	float rangScale;
	int getBtnIdx(float btnVal) {
		return roundFloatToInt(rangScale * (btnVal - range->start));
	}
	float btnIdxToValue(int btnIdx) {
		return range->start + (float)btnIdx * range->interval;
	}

	bool ignoreCallbacks;
	CriticalSection selfCallbackMutex;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SbRadioGroupAttachment)
};

