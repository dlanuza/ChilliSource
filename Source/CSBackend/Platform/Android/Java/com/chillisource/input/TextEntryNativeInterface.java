/**
 * TextEntryNativeInterface.java
 * Chilli Source
 * Created by Scott Downie 08/07/2014.
 * 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Tag Games Limited
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

package com.chillisource.input;

import android.content.Context;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import com.chillisource.core.CSApplication;
import com.chillisource.core.Logging;
import com.chillisource.core.InterfaceIDType;
import com.chillisource.core.INativeInterface;

/**
 * Native interface that wraps a single text entry entity. The system 
 * will track user text entry and notify native about any changes to the
 * text buffer
 * 
 * @author S Downie
 *
 */
public class TextEntryNativeInterface extends INativeInterface implements TextWatcher, OnEditorActionListener
{
	public static final InterfaceIDType InterfaceID = new InterfaceIDType("CKeyboardNativeInterface");
	private int m_keyboardTypeFlags = 0;
	private int m_keyboardCapitalisationFlags = 0;
	private EditTextBackEvent m_textEntryView;
	
	/**
	 * Extends EditText in order to detect when the keyboard has been dismissed
	 * 
	 * @author S Downie
	 */
	private class EditTextBackEvent extends EditText
	{
		private TextEntryNativeInterface m_nativeInterface;
		
		/**
		 * Constructor
		 * 
		 * @author S Downie
		 * 
		 * @param Context
		 * @param Text entry interface to notify when keyboard dismissed
		 */
		public EditTextBackEvent(Context in_context, TextEntryNativeInterface in_nativeInterface) 
		{
			super(in_context);
			m_nativeInterface = in_nativeInterface;
		}
		/**
		 * Called when the keyboard key event occurs prior to any system being notified
		 * 
		 * @author S Downie
		 * 
		 * @param key code
		 * @param key event
		 * 
		 * @return Whether the event has been handled
		 */
        @Override public boolean onKeyPreIme(int in_keyCode, KeyEvent in_event)
        {
            if (in_event.getKeyCode() == KeyEvent.KEYCODE_BACK || in_event.getKeyCode() == KeyEvent.FLAG_EDITOR_ACTION || in_event.getKeyCode() == KeyEvent.KEYCODE_ENTER)
            {
				m_nativeInterface.Dismiss();
            	m_nativeInterface.NativeOnKeyboardDismissed();
            	return true;
            }
            
            return super.onKeyPreIme(in_keyCode, in_event);
        }
	}

	/** 
	 * Constructor
	 * 
	 * @author S Downie
	 */
	TextEntryNativeInterface()
	{
		//Doesn't matter what we set this to it's invisible
		m_textEntryView = new EditTextBackEvent(CSApplication.get().getActivityContext(), this);
		m_textEntryView.setWidth(100);
		m_textEntryView.setHeight(100);
		m_textEntryView.setAlpha(0);
		m_textEntryView.addTextChangedListener(this);
		m_textEntryView.setOnEditorActionListener(this);
	}
	/**
	 * @author Ian Copland
	 * 
	 * @param Interface type
	 * 
	 * @return Whether this objects interface is of the given type
	 */
	@Override public boolean IsA(InterfaceIDType in_interfaceType) 
	{
		return (in_interfaceType == InterfaceID);
	}
	/**
	 * Activate text entry. This will display the soft keyboard if required
	 * 
	 * @author S Downie
	 */
	public void Activate()
	{
		Runnable task = new Runnable() 
		{
			@Override public void run() 
			{
				CSApplication.get().addView(m_textEntryView);
				m_textEntryView.requestFocus();
				m_textEntryView.setSelection(m_textEntryView.getText().length());
				InputMethodManager imm = (InputMethodManager)CSApplication.get().getAppContext().getSystemService(Context.INPUT_METHOD_SERVICE);
				imm.showSoftInput(m_textEntryView, InputMethodManager.SHOW_IMPLICIT);
			}
		};
		CSApplication.get().scheduleUIThreadTask(task);
	}
	/**
	 * Deactivate text entry. This will hide the soft keyboard if required
	 * 
	 * @author S Downie
	 */
	public void Deactivate()
	{
		Runnable task = new Runnable() 
		{
			@Override public void run() 
			{
				Dismiss();
			}
		};
		CSApplication.get().scheduleUIThreadTask(task);
	}
	/**
	 * Dismiss the keyboard
	 * 
	 * @author S Downie
	 */
	private void Dismiss()
	{
		ViewGroup parent = (ViewGroup)m_textEntryView.getParent();
		if (parent != null)
		{
			parent.removeView(m_textEntryView);
			InputMethodManager imm = (InputMethodManager)CSApplication.get().getAppContext().getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.toggleSoftInput(0, 0);
		}
	}
	/**
	 * Sets the keyboard type that should be used the next the keyboard is displayed.
	 * 
	 * @author Ian Copland
	 * 
	 * @param Type as an integer (so it can be passed from native)
	 */
    public void SetKeyboardType(int in_type)
    {
    	m_keyboardTypeFlags = IntegerToKeyboardType(in_type);
    	m_textEntryView.setInputType(m_keyboardTypeFlags | m_keyboardCapitalisationFlags);
    }
	/**
	 * Sets the capitalisation method of the text buffer.
	 * 
	 * @author Ian Copland
	 * 
	 * @param Type as an integer (so it can be passed from native)
	 */
    public void SetCapitalisationMethod(int in_method)
    {
    	m_keyboardCapitalisationFlags = IntegerToKeyboardCapitalisation(in_method);
    	m_textEntryView.setInputType(m_keyboardTypeFlags | m_keyboardCapitalisationFlags);
    }
	/**
	 * @author Ian Copland
	 *
	 * @param The keyboard type as an CS integer 
	 *
	 * @return The input type flag required by Android.
	 */
	private int IntegerToKeyboardType(int in_type)
	{
		switch (in_type)
		{
			case 0:
				return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
			case 1:
				return InputType.TYPE_CLASS_NUMBER;
			default:
				Logging.logError("Invalid keyboard type integer, cannot be converted.");
				return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
		}
	}
	/**
	 * @author Ian Copland
	 *
	 * @param The capitalisation method as an CS integer 
	 *
	 * @return The capitalisation input type flag required by Android.
	 */
	private int IntegerToKeyboardCapitalisation(int in_method)
	{
		switch (in_method)
		{
			case 0:
				return 0;
			case 1:
				return InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
			case 2:
				return InputType.TYPE_TEXT_FLAG_CAP_WORDS;
			case 3:
				return InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS;
			default:
				Logging.logError("Invalid keyboard capitalisation integer, cannot be converted.");
				return 0;
		}
	}
	/** 
	 * @author S Downie
	 * 
	 * @param The contents of the text buffer
	 */
	public void setTextBuffer(final String in_text)
	{
		Runnable task = new Runnable() 
		{
			@Override public void run() 
			{
				m_textEntryView.setText(in_text);
			}
		};
		CSApplication.get().scheduleUIThreadTask(task);
	}
	/**
	 * Tells the native app that text has changed
	 * 
	 * @author S Downie
	 * 
	 * @param New text
	 */
	native private void NativeOnTextChanged(String in_text);
	/**
	 * A native call for passing the keyboard dismissed
	 * event down to the native side of the engine.
	 * 
	 * @author S Downie
	 */
	native private void NativeOnKeyboardDismissed();
	/**
	 * Called when the text string of the given editable has
	 * changed
	 * 
	 * @author S Downie
	 * 
	 * @param Editable that has changed
	 */
	@Override public void afterTextChanged(Editable in_editable) 
	{

	}
	/**
	 * Called when the text string of the given editable is
	 * about to change
	 * 
	 * @author S Downie
	 * 
	 * @param String that is about to change
	 * @param First character index that is about to change
	 * @param Number of characters to change from start
	 * @param Length of string after change
	 */
	@Override public void beforeTextChanged(CharSequence in_string, int in_start, int in_count, int in_newLength) 
	{

	}
	/**
	 * Called when the text string of the given editable is
	 * about to change
	 * 
	 * @author S Downie
	 * 
	 * @param String that has changed
	 * @param First character index that changed
	 * @param Number of characters changed from start
	 * @param Length of string after change
	 */
	@Override public void onTextChanged(CharSequence in_string, int in_start, int in_count, int in_newLength) 
	{
		NativeOnTextChanged(in_string.toString());
	}
	/**
	 * Called when the an IME key event
	 * 
	 * @author S Downie
	 * 
	 * @param Text view on which action occurred
	 * @param Action code
	 * @param Key event
	 * 
	 * @return Whether the event has been handled
	 */
	@Override public boolean onEditorAction(TextView in_view, int in_actionId, KeyEvent in_event)
	{
        if (in_actionId == EditorInfo.IME_ACTION_DONE || in_actionId == EditorInfo.IME_ACTION_GO || in_actionId == EditorInfo.IME_ACTION_NEXT)
        {
			Dismiss();
        	NativeOnKeyboardDismissed();
        	return true;
        }
        
		return false;
	}
}
