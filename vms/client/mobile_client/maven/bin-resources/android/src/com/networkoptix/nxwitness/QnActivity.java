package com.networkoptix.nxwitness;

import android.os.Bundle;
import org.qtproject.qt5.android.bindings.QtActivity;
import com.networkoptix.nxwitness.utils.QnWindowUtils;

public class QnActivity extends QtActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        QnWindowUtils.prepareSystemUi();
    }

}
