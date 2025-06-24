#include "settings.hh"

#include "ads_bee.hh"
#include "eeprom.hh"

const char SettingsManager::ConsoleLogLevelStrs[SettingsManager::LogLevel::kNumLogLevels]
                                               [SettingsManager::kConsoleLogLevelStrMaxLen] = {"SILENT", "ERRORS",
                                                                                               "WARNINGS", "INFO"};
const char SettingsManager::SerialInterfaceStrs[SettingsManager::SerialInterface::kNumSerialInterfaces]
                                               [SettingsManager::kSerialInterfaceStrMaxLen] = {"CONSOLE", "COMMS_UART",
                                                                                               "GNSS_UART"};
const char SettingsManager::ReportingProtocolStrs[SettingsManager::ReportingProtocol::kNumProtocols]
                                                 [SettingsManager::kReportingProtocolStrMaxLen] = {
                                                     "NONE", "RAW", "RAW_VALIDATED", "MAVLINK1", "MAVLINK2", "GDL90"};

bool SettingsManager::Load() {
    if (!eeprom.Load(settings)) {
        CONSOLE_ERROR("settings.h::Load: Failed load settings from EEPROM.");
        return false;
    };

    // Reset to defaults if loading from a blank EEPROM.
    if (settings.magic_word != 0xDEADBEEF) {
        ResetToDefaults();
        if (!eeprom.Save(settings)) {
            CONSOLE_ERROR("settings.h::Load: Failed to save default settings.");
            return false;
        }
    }

    Apply();

    return true;
}

bool SettingsManager::Save() {
    settings.rx_gain = ads_bee.GetRxGain();
    settings.tl_lo_mv = ads_bee.GetTLLoMilliVolts();
    settings.tl_hi_mv = ads_bee.GetTLHiMilliVolts();

    // Save reporting protocols.
    comms_manager.GetReportingProtocol(SerialInterface::kCommsUART,
                                       settings.reporting_protocols[SerialInterface::kCommsUART]);
    comms_manager.GetReportingProtocol(SerialInterface::kConsole,
                                       settings.reporting_protocols[SerialInterface::kConsole]);

    // Save baud rates.
    comms_manager.GetBaudrate(SerialInterface::kCommsUART, settings.comms_uart_baud_rate);
    comms_manager.GetBaudrate(SerialInterface::kGNSSUART, settings.gnss_uart_baud_rate);

    // Save WiFi configuration.
    settings.wifi_enabled = comms_manager.WiFiIsEnabled();
    strncpy(settings.wifi_ssid, comms_manager.wifi_ssid, kWiFiSSIDMaxLen);
    settings.wifi_ssid[kWiFiSSIDMaxLen] = '\0';
    strncpy(settings.wifi_password, comms_manager.wifi_password, kWiFiPasswordMaxLen);
    settings.wifi_password[kWiFiPasswordMaxLen] = '\0';

    return eeprom.Save(settings);
}

void SettingsManager::ResetToDefaults() {
    Settings default_settings;
    settings = default_settings;
    Apply();
}

void SettingsManager::Apply() {
    ads_bee.SetTLLoMilliVolts(settings.tl_lo_mv);
    ads_bee.SetTLHiMilliVolts(settings.tl_hi_mv);
    ads_bee.SetRxGain(settings.rx_gain);

    // Save reporting protocols.
    comms_manager.SetReportingProtocol(SerialInterface::kCommsUART,
                                       settings.reporting_protocols[SerialInterface::kCommsUART]);
    comms_manager.SetReportingProtocol(SerialInterface::kConsole,
                                       settings.reporting_protocols[SerialInterface::kConsole]);

    // Save baud rates.
    comms_manager.SetBaudrate(SerialInterface::kCommsUART, settings.comms_uart_baud_rate);
    comms_manager.SetBaudrate(SerialInterface::kGNSSUART, settings.gnss_uart_baud_rate);

    // Set WiFi configurations.
    comms_manager.SetWiFiEnabled(settings.wifi_enabled);
    strncpy(comms_manager.wifi_ssid, settings.wifi_ssid, kWiFiSSIDMaxLen);
    comms_manager.wifi_ssid[kWiFiSSIDMaxLen] = '\0';
    strncpy(comms_manager.wifi_password, settings.wifi_password, kWiFiPasswordMaxLen);
    comms_manager.wifi_password[kWiFiPasswordMaxLen] = '\0';
}