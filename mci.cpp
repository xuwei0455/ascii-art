#include "mci.h"
#include "stdio.h"
#include <locale>
#include <codecvt>
#include <mmstream.h>
#pragma comment(lib,"winmm.lib")


WCHAR buf[1024];

UINT device_id;
MCI_OPEN_PARMS mci_open_params;
MCIERROR mci_error;


void
Player::play(std::string& file_path)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    auto path = std::wstring(converter.from_bytes(file_path));
    this->_path = path.c_str();

    mci_open_params.lpstrDeviceType = this->_device_type;
    mci_open_params.lpstrElementName = this->_path;

    mci_error = mciSendCommand(NULL, MCI_OPEN, MCI_OPEN_ELEMENT, reinterpret_cast<DWORD_PTR>(&mci_open_params));
    if (mci_error)
    {
        mciGetErrorString(mci_error, buf, sizeof(buf));
        this->_str_message = L"Send MCI_OPEN command failed!";
    }
    else
    {
        this->_str_message = L"Media open success!";
    }

    const auto device_id = mci_open_params.wDeviceID;
    MCI_PLAY_PARMS mci_play;
    mci_error = mciSendCommand(device_id, MCI_PLAY, 0, reinterpret_cast<DWORD_PTR>(&mci_play));
    if (mci_error)
    {
        this->_str_message = L"send MCI_PLAY command failed!";
    }
    else
    {
        WCHAR buf[1024] = {};
        swprintf_s(buf, L"Now playing %s!", this->_path);
        this->_str_message = buf;
        this->_state = PLAY;
    }
}


void
Player::pause()
{
    const auto device_id = mci_open_params.wDeviceID;
    MCI_PLAY_PARMS mci_play_params;

    mci_error = mciSendCommand(device_id, MCI_PAUSE, 0, reinterpret_cast<DWORD_PTR>(&mci_play_params));
    if (mci_error)
    {
        this->_str_message = L"send MCI_PAUSE command failed!";
    }
    else
    {
        this->_str_message = L"Media play pause!";
        this->_state = PAUSE;
    }
}

void
Player::resume()
{
    const auto device_id = mci_open_params.wDeviceID;
    MCI_PLAY_PARMS mci_play_params;

    mci_error = mciSendCommand(device_id, MCI_RESUME, MCI_WAIT, reinterpret_cast<DWORD_PTR>(&mci_play_params));
    if (mci_error)
    {
        this->_str_message = L"Send MCI_RESUME command faild!";
    }
    else
    {
        this->_str_message = L"Media paly resumed!";
        this->_state = PLAY;
    }
}

void Player::stop()
{
    const UINT device_id = mci_open_params.wDeviceID;

    mci_error = mciSendCommand(device_id, MCI_STOP, NULL, NULL);
    if (mci_error)
    {
        this->_str_message = L"Send MCI_STOP command failed!";
    }
    else
    {
        this->_str_message = L"Media play stopped!";
        this->_state = STOP;
    }
}

void Player::close()
{
    const auto device_id = mci_open_params.wDeviceID;

    MCI_GENERIC_PARMS gp;
    gp.dwCallback = NULL;

    mci_error = mciSendCommand(device_id, MCI_CLOSE, MCI_WAIT, reinterpret_cast<DWORD_PTR>(&gp));//¹Ø±Õ²¥·Å
    if (mci_error)
    {
        this->_str_message = L"Send MCI_CLOSE command faild";
    }
    else
    {
        this->_str_message = L"media player closed!";
        this->_state = STOP;
    }
}


DWORD
Player::get_state()
{
    const auto device_id = mci_open_params.wDeviceID;
    MCI_STATUS_PARMS status_parms;
    status_parms.dwItem = MCI_STATUS_MODE;

    mciSendCommand(device_id, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM, reinterpret_cast<DWORD_PTR>(&status_parms));
    return status_parms.dwReturn;
}
