// cafap-rest-api-client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>

#include "curl/curl.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

static char errorBuffer[CURL_ERROR_SIZE];
static std::string buffer;

static int writer(char *data, size_t size, size_t nmemb, std::string *buffer)
{
	int result = 0;
	if (buffer != NULL)
	{
		buffer->append(data, size * nmemb);
		result = (int)(size * nmemb);
	}
	return result;
}

static size_t OnReceiveData(void * pData, size_t tSize, size_t tCount, void * pmUser)
{
	size_t length = tSize * tCount, index = 0;
	while (index < length)
	{
		unsigned char *temp = (unsigned char *)pData + index;
		if ((temp[0] == '\r') || (temp[0] == '\n'))
			break;
		index++;
	}

	std::string str((unsigned char*)pData, (unsigned char*)pData + index);
	std::map<std::string, std::string>* pmHeader = (std::map<std::string, std::string>*)pmUser;
	size_t pos = str.find(": ");
	if (pos != std::string::npos)
		pmHeader->insert(std::pair<std::string, std::string>(str.substr(0, pos), str.substr(pos + 2)));

	return (tCount);
}

bool ParseHTTPResult(std::string & json,
	bool & api_error,
	std::vector<std::string> & api_errorMsg,
	long long & api_Id
)
{
	rapidjson::Document JSONDocument;
	if (JSONDocument.Parse(json.c_str()).HasParseError())
	{
		std::cout << "Сервис вернул ответ не в формате JSON! .. exiting\n";
		return false;
	}
	// Ожидается ответ такого вида
	/*
	{
	 error: true or false,
	 errorMsg : [ strings ] if error is true,
	 Id : number
	}
	*/

	const rapidjson::Value & JSObject = JSONDocument;
	// Корень должен быть Object
	if (JSObject.IsObject())
	{
		for (rapidjson::Value::ConstMemberIterator itr = JSObject.MemberBegin(); itr != JSObject.MemberEnd(); ++itr)
		{
			const rapidjson::Value & JSDataValue = itr->value;
			if (!JSDataValue.IsNull()) {
				std::string DataValueName = itr->name.GetString();
				if (DataValueName.compare("error") == 0)
				{
					if (JSDataValue.IsBool()) {
						api_error = JSDataValue.GetBool();
					} else {
						std::cout << "error field expected to be boolean type .. exiting\n";
						return false;
					}
				} else if (DataValueName.compare("errorMsg") == 0)
				{
					if (JSDataValue.IsArray()) {
						for (rapidjson::Value::ConstValueIterator msgValue = JSDataValue.Begin(); msgValue != JSDataValue.End(); ++msgValue)
						{
							if (!msgValue->IsNull()) {
								if (msgValue->IsString())
								{
									std::string txtValue = msgValue->GetString();
									api_errorMsg.push_back(txtValue);
								} else {
									std::cout << "member of errorMsg array expected to be string .. exiting\n";
									return false;
								}
							}
						}
					} else {
						std::cout << "errorMsg expected to be Array.. exiting\n";
						return false;
					}

				} else if (JSDataValue.IsInt64()) {
					api_Id = JSDataValue.GetInt64();
				}
			}
		}
	}

	return true;
}

bool do_connect(
	bool & api_error,
	std::vector<std::string> & api_errorMsg,
	long long & sessionId
) {
	bool isSucces = false;
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_URL, "http://10.146.33.17/cafap-api/connect");

		// curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		// curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, OnReceiveData);
		std::map<std::string, std::string> mHeader;
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &mHeader);

		//указываем функцию обратного вызова для записи получаемых данных
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

		//указываем куда записывать принимаемые данные
		//объявляем буфер принимаемых данных
		buffer.clear();
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		struct curl_slist *headers = NULL;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		curl_mime *mime;
		curl_mimepart *part;
		mime = curl_mime_init(curl);
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "dislId");
		curl_mime_data(part, "13880", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "sensorName");
		curl_mime_data(part, "UR_2008001", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "userTxt");
		curl_mime_data(part, "Вася Иванов", CURL_ZERO_TERMINATED);
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
		res = curl_easy_perform(curl);

		//проверяем успешность выполнения операции
		if (res == CURLE_OK)
		{
			// Прверяем RESPONSE CODE
			long response_code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
			std::cout << "connect response code is " << response_code << "\n";
			std::cout << "connect response data is " << buffer << "\n";
			switch (response_code)
			{
			case 200: // OK, let's process response
				break;
			case 400: // User Error
				break;
			case 500: // Server Error
				break;
			default:
				break;
			}
			isSucces = ParseHTTPResult(
				// Полученные данные
				buffer,

				// returning value
				api_error, api_errorMsg, sessionId
			);
		} else {
			std::cout << errorBuffer << "\n";
		}

		curl_mime_free(mime);
	}
	curl_easy_cleanup(curl);

	return isSucces;
}

bool do_addevent(const long long & sessionId,
	bool & api_error,
	std::vector<std::string> & api_errorMsg,
	long long & eventId
) {
	bool isSucces = false;
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_URL, "http://10.146.33.17/cafap-api/addevent");

		// curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		// curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, OnReceiveData);
		std::map<std::string, std::string> mHeader;
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &mHeader);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

		buffer.clear();
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		struct curl_slist *headers = NULL;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		curl_mime *mime;
		curl_mimepart *part;
		mime = curl_mime_init(curl);
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "sessionId");
		std::string str_sessionId = std::to_string(sessionId);
		curl_mime_data(part, str_sessionId.c_str(), CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "eventDateTime");
		curl_mime_data(part, "2020-10-28 09:01:02", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "movementDirection");
		curl_mime_data(part, "Попутное", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "fixedSpeed");
		curl_mime_data(part, "90", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "recognisedLicenseNumber");
		curl_mime_data(part, "Й001ЦУ", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "recognisedRegionNumber");
		curl_mime_data(part, "116", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "assuranceValue");
		curl_mime_data(part, "75", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "channelNum");
		curl_mime_data(part, "Средняя полоса", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "xLU");
		curl_mime_data(part, "1", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "yLU");
		curl_mime_data(part, "1", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "xRD");
		curl_mime_data(part, "100", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "yRD");
		curl_mime_data(part, "100", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "latitude");
		curl_mime_data(part, "6.1234", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		curl_mime_name(part, "longitude");
		curl_mime_data(part, "7.43221", CURL_ZERO_TERMINATED);
		part = curl_mime_addpart(mime);

		std::ifstream inputPhotoPanoramic("photoPanoramic.png", std::ios::binary);
		std::string photoPanoramicData(std::istreambuf_iterator<char>(inputPhotoPanoramic), {});
		curl_mime_name(part, "photoPanoramic");
		curl_mime_data(part, photoPanoramicData.c_str(), photoPanoramicData.length());
		curl_mime_filename(part, "photoPanoramic.png");
		part = curl_mime_addpart(mime);

		std::ifstream inputPhotoLicensePlate("photoLicensePlate.jpg", std::ios::binary);
		std::string photoLicensePlateData(std::istreambuf_iterator<char>(inputPhotoLicensePlate), {});
		curl_mime_name(part, "photoLicensePlate");
		curl_mime_data(part, photoLicensePlateData.c_str(), photoLicensePlateData.length());
		curl_mime_filename(part, "photoLicensePlate.jpg");
		part = curl_mime_addpart(mime);

		curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

		res = curl_easy_perform(curl);
		if (res == CURLE_OK)
		{
			long response_code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
			std::cout << "addevent response code is " << response_code << "\n";
			std::cout << "connect response data is " << buffer << "\n";
			switch (response_code)
			{
			case 200: // OK, let's process response
				break;
			case 400: // User Error
				break;
			case 500: // Server Error
				break;
			default:
				break;
			}
			isSucces = ParseHTTPResult(buffer,
				api_error, api_errorMsg, eventId
			);
		} else {
			std::cout << errorBuffer << "\n";
		}

		curl_mime_free(mime);
	}
	curl_easy_cleanup(curl);

	return isSucces;
}

void showErrors(std::vector<std::string> & api_errorMsg)
{
	for (std::vector<std::string>::iterator it = api_errorMsg.begin(); it != api_errorMsg.end(); ++it)
		std::cout << ' ' << *it << "\n";
}

int main()
{
	std::cout << "Started\n";
	bool api_error = true;
	std::vector<std::string> api_errorMsg;
	long long sessionId = 0;

	bool isSuccess = do_connect(api_error, api_errorMsg, sessionId);
	if (isSuccess) {
		if (api_error) {
			std::cout << "error returned from connect: \n";
			showErrors(api_errorMsg);
		} else {
			std::cout << "connect returned sessionId = " << sessionId << "\n";
		}
	}
	long long eventId = 0;
	if (isSuccess) isSuccess = do_addevent(sessionId, api_error, api_errorMsg, eventId);
	if (isSuccess) {
		if (api_error) {
			std::cout << "error returned from addevent: \n";
			showErrors(api_errorMsg);
		} else {
			std::cout << "addevent returned eventId = " << eventId << "\n";
		}
	}

	std::cout << "Finished\n";
}
