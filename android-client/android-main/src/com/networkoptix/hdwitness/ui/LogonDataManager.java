package com.networkoptix.hdwitness.ui;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

import com.networkoptix.hdwitness.common.SimpleCrypto;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * Utility class for login credentials management. Stores set of all recently
 * used connections in the Android Preferences.
 * 
 * @author gdm
 * 
 */
public class LogonDataManager {

    private static final String ENTRY_COUNT = "entry_count";

    public static class LogonEntry implements Serializable {
        
        public String Title;
        public String Login;
        public String Password;
        public String Hostname;
        public int Port;
        
        private static final long serialVersionUID = -3667692511338792047L;

        private static final String cryptoSeed = "ILyBoxYJ5H";

        private static final String ENTRY_TITLE = "entry_title";
        private static final String ENTRY_LOGIN = "entry_login";
        private static final String ENTRY_PASSWORD = "entry_password";
        private static final String ENTRY_HOST = "entry_host";
        private static final String ENTRY_PORT = "entry_port";

        public LogonEntry(SharedPreferences prefs, int idx) {
            String encryptedLogin = prefs.getString(ENTRY_LOGIN + idx, null);
            Login = SimpleCrypto.decryptSafe(cryptoSeed, encryptedLogin, null);

            String encryptedPassword = prefs.getString(ENTRY_PASSWORD + idx, null);
            Password = SimpleCrypto.decryptSafe(cryptoSeed, encryptedPassword, null);

            Title = prefs.getString(ENTRY_TITLE + idx, null);
            Hostname = prefs.getString(ENTRY_HOST + idx, null);
            Port = prefs.getInt(ENTRY_PORT + idx, 0);
        }

        public LogonEntry(String title, String userName, String password, String hostName, int port) {
            Title = title;
            Login = userName;
            Password = password;
            Hostname = hostName;
            Port = port;
        }

        public void save(Editor editor, int idx) {
            editor.putString(ENTRY_LOGIN + idx, SimpleCrypto.encryptSafe(cryptoSeed, Login, null));
            editor.putString(ENTRY_PASSWORD + idx, SimpleCrypto.encryptSafe(cryptoSeed, Password, null));
            editor.putString(ENTRY_TITLE + idx, Title);
            editor.putString(ENTRY_HOST + idx, Hostname);
            editor.putInt(ENTRY_PORT + idx, Port);
        }

        public boolean isValid() {
            return Login != null && (Login.length() > 0) && Password != null && (Password.length() > 0)
                    && Hostname != null && (Hostname.length() > 0) && Port > 0;
        }

        public void copyFrom(LogonEntry other) {
            Title = other.Title;
            Login = other.Login;
            Password = other.Password;
            Hostname = other.Hostname;
            Port = other.Port;
        }

        public String title() {
            String result = Title;
            if (result == null || result.length() == 0)
                result = Hostname;
            if (result == null)
                result = "";
            return result;
        }
    }

    private List<LogonEntry> mEntries = new ArrayList<LogonEntry>();

    public LogonDataManager() {
    }

    /**
     * Loads stored data from preferences to memory.
     */
    public void init(SharedPreferences prefs) {
        mEntries.clear();
        int count = prefs.getInt(ENTRY_COUNT, 0);
        for (int i = 0; i < count; i++) {
            LogonEntry e = new LogonEntry(prefs, i);
            if (e.isValid())
                mEntries.add(e);
        }
    }

    /**
     * Saves stored data to preferences.
     */
    public void store(SharedPreferences.Editor editor) {
        editor.putInt(ENTRY_COUNT, mEntries.size());
        for (int i = 0; i < mEntries.size(); i++) {
            mEntries.get(i).save(editor, i);
        }
    }

    public void addEntry(LogonEntry entry) {
        mEntries.add(0, entry);
    }
    
    /**
     * Creates new entry with the provided login credentials and stores it in
     * the inner structure. If the entry with same title is already exists it
     * will be replaced. Added entry will be marked as most recently used.
     * 
     * @param userName
     *            - login
     * @param password
     *            - password
     * @param hostName
     *            - hostname
     * @param port
     *            - port
     */
    public void addEntry(String title, String userName, String password, String hostName, int port) {
        LogonEntry entry = new LogonEntry(title, userName, password, hostName, port);
        if (!entry.isValid())
            return;
        addEntry(entry);
    }

    public List<LogonEntry> getEntries() {
        return mEntries;
    }

    public void removeEntry(int index) {
        mEntries.remove(index);
    }

    public void updateEntry(int index, LogonEntry entry) {
        if (index < 0)
            return;
        mEntries.get(index).copyFrom(entry);
    }

    

}
