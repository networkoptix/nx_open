package com.networkoptix.hdwitness.internal;

/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import android.content.Context;
import android.view.Window;

/**
 * Replacement class for an Android internal PolicyManager class.
 */
public final class PolicyManager {
	private static final String POLICY_IMPL_CLASS_NAME = "com.android.internal.policy.impl.Policy";	
	private static final String POLICY_IMPL_METHOD_NAME = "makeNewWindow";
	private static final String PHONE_WINDOW_CLASS_NAME = "com.android.internal.policy.PhoneWindow";


	// Cannot instantiate this class
	private PolicyManager() {
	}

	// The static methods to spawn new policy-specific objects
	public static Window makeNewWindow(Context context) {    
	    try {
	        @SuppressWarnings("rawtypes")
            Class windowClass = Class.forName(PHONE_WINDOW_CLASS_NAME);
	        @SuppressWarnings("unchecked")
            final Window w = (Window) windowClass.getConstructor(Context.class).newInstance(context);
	        return w;
	    } catch (Exception ex) {
	        //just skip and try the legacy code
        } 	    
	    
		Method m;
		Object policy;
		try {
		    @SuppressWarnings("rawtypes")
            Class policyClass = Class.forName(POLICY_IMPL_CLASS_NAME);
		    policy = policyClass.newInstance();
			m = policy.getClass().getMethod(POLICY_IMPL_METHOD_NAME, Context.class);
		} catch (Exception ex) {
		    return null;
		}
		
		try {
			return (Window) m.invoke(policy, context);
		} catch (Exception ex) {
            return null;
        } 
	}

}
