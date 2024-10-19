// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.push.firebase;

import androidx.annotation.NonNull;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.messaging.FirebaseMessaging;
import com.nxvms.mobile.utils.Logger;


public class FirebaseTokenHelper
{
    private static final String kLogTag = "FirebaseTokenHelper";

    private static native void onComplete(int callbackId, boolean success, String value);

    public static void getToken(int callbackId)
    {
        FirebaseMessaging.getInstance().getToken()
            .addOnCompleteListener(new OnCompleteListener<String>()
            {
                @Override
                public void onComplete(@NonNull Task<String> task)
                {
                    if (!task.isSuccessful())
                    {
                        FirebaseTokenHelper.onComplete(callbackId, false, "");
                        return;
                    }

                    FirebaseTokenHelper.onComplete(callbackId, true, task.getResult());
                }
            });
    }

    public static void deleteToken(int callbackId)
    {
        FirebaseMessaging.getInstance().deleteToken()
            .addOnCompleteListener(new OnCompleteListener<Void>()
            {
                @Override
                public void onComplete(@NonNull Task<Void> task) {
                    FirebaseTokenHelper.onComplete(callbackId, task.isSuccessful(), "");
                }
            });
    }
}
