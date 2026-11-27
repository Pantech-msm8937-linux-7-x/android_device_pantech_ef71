/*
 * Copyright (C) 2026 The LineageOS Project
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

package org.lineageos.settings.zram;

import android.app.AlertDialog;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.SystemProperties;

import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class ZramSettingsFragment extends PreferenceFragment
        implements Preference.OnPreferenceChangeListener {

    /* Read by init when it sets up the swap, in GB */
    private static final String PROP_ZRAM_SIZE = "persist.sys.zram_size";
    private static final String DEFAULT_SIZE = "4";

    private static final String ZRAM_SYSFS = "/sys/block/zram0/";

    private ListPreference mSize;
    private Preference mUsage;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.zram_settings);

        mSize = findPreference("zram_size");
        mSize.setValue(SystemProperties.get(PROP_ZRAM_SIZE, DEFAULT_SIZE));
        mSize.setOnPreferenceChangeListener(this);
        updateSizeSummary();

        mUsage = findPreference("zram_usage");
    }

    @Override
    public void onResume() {
        super.onResume();
        updateUsage();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String value = (String) newValue;
        if (value.equals(mSize.getValue()))
            return true;

        SystemProperties.set(PROP_ZRAM_SIZE, value);
        mSize.setValue(value);
        updateSizeSummary();

        new AlertDialog.Builder(getContext())
                .setMessage(R.string.zram_restart_message)
                .setPositiveButton(R.string.zram_restart, (dialog, which) ->
                        getContext().getSystemService(PowerManager.class).reboot(null))
                .setNegativeButton(R.string.zram_restart_later, null)
                .show();
        return false;
    }

    private void updateSizeSummary() {
        long chosen = Long.parseLong(mSize.getValue()) << 30;
        long current = readLong(ZRAM_SYSFS + "disksize", 0);
        if (chosen == current)
            mSize.setSummary(mSize.getEntry());
        else
            mSize.setSummary(getString(R.string.zram_size_summary_pending, mSize.getEntry()));
    }

    /* In binary units, as the sizes to choose from are */
    private static String formatSize(long bytes) {
        if (bytes >= 1L << 30)
            return String.format("%.1f GB", bytes / (double) (1L << 30));
        if (bytes >= 1L << 20)
            return String.format("%.1f MB", bytes / (double) (1L << 20));
        return String.format("%d KB", bytes >> 10);
    }

    private void updateUsage() {
        long disk = readLong(ZRAM_SYSFS + "disksize", 0);
        if (disk == 0) {
            mUsage.setSummary(R.string.zram_usage_off);
            return;
        }

        /* orig_data_size compr_data_size mem_used_total ... */
        String[] mm = readLine(ZRAM_SYSFS + "mm_stat").trim().split("\\s+");
        long orig = mm.length > 2 ? Long.parseLong(mm[0]) : 0;
        long used = mm.length > 2 ? Long.parseLong(mm[2]) : 0;

        mUsage.setSummary(getString(R.string.zram_usage_summary,
                formatSize(orig), formatSize(disk), formatSize(used)));
    }

    private static String readLine(String path) {
        try (BufferedReader reader = new BufferedReader(new FileReader(path))) {
            String line = reader.readLine();
            return line != null ? line : "";
        } catch (IOException e) {
            return "";
        }
    }

    private static long readLong(String path, long def) {
        try {
            return Long.parseLong(readLine(path).trim());
        } catch (NumberFormatException e) {
            return def;
        }
    }
}
