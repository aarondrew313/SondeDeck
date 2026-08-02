#pragma once

enum class DisplayPage {
    Home,
    Overview,
    Sonde,
    Navigation,
    LocalGps,
    Logging,
    Power,
    Frequency,
    Online,
    Help,
    HelpStatus,
    About
};

inline bool isMainDisplayPage(DisplayPage page) {
    return page == DisplayPage::Overview ||
           page == DisplayPage::Sonde ||
           page == DisplayPage::Navigation ||
           page == DisplayPage::LocalGps ||
           page == DisplayPage::Logging ||
           page == DisplayPage::Power ||
           page == DisplayPage::Frequency ||
           page == DisplayPage::Online;
}

inline DisplayPage nextDisplayPage(DisplayPage page) {
    switch (page) {
        case DisplayPage::Home:
            return DisplayPage::Overview;
        case DisplayPage::Overview:
            return DisplayPage::Sonde;
        case DisplayPage::Sonde:
            return DisplayPage::Navigation;
        case DisplayPage::Navigation:
            return DisplayPage::LocalGps;
        case DisplayPage::LocalGps:
            return DisplayPage::Logging;
        case DisplayPage::Logging:
            return DisplayPage::Power;
        case DisplayPage::Power:
            return DisplayPage::Frequency;
        case DisplayPage::Frequency:
            return DisplayPage::Online;
        case DisplayPage::Online:
            return DisplayPage::Overview;
        case DisplayPage::Help:
        case DisplayPage::HelpStatus:
        case DisplayPage::About:
        default:
            return DisplayPage::Home;
    }
}

inline DisplayPage previousDisplayPage(DisplayPage page) {
    switch (page) {
        case DisplayPage::Home:
            return DisplayPage::Online;
        case DisplayPage::Overview:
            return DisplayPage::Online;
        case DisplayPage::Sonde:
            return DisplayPage::Overview;
        case DisplayPage::Navigation:
            return DisplayPage::Sonde;
        case DisplayPage::LocalGps:
            return DisplayPage::Navigation;
        case DisplayPage::Logging:
            return DisplayPage::LocalGps;
        case DisplayPage::Power:
            return DisplayPage::Logging;
        case DisplayPage::Frequency:
            return DisplayPage::Power;
        case DisplayPage::Online:
            return DisplayPage::Frequency;
        case DisplayPage::Help:
        case DisplayPage::HelpStatus:
        case DisplayPage::About:
        default:
            return DisplayPage::Home;
    }
}
