#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <functional>
#include <windows.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <rapidxml/rapidxml.hpp>
#include <cppcodec/base64_default_rfc4648.hpp>

//Initializig some bullshit
CURL* curl;
CURLcode res;
std::string data;
std::string line;
std::string filename;
std::vector<std::string> groupIDs;
std::vector<std::string> steamIDs;
int totalPages = 1;
int currentPage = 0;
int totalUsers = 0;
int maxLines = 70;
int lineCount = 0;
int choice;

//Generate a random web key to prevent rate limiting
std::string webKeys[3] = {
	"5DA40A4A4699DEE30C1C9A7BCE84C914",
	"5970533AA2A0651E9105E706D0F8EDDC",
	"2B3382EBA9E8C1B58054BD5C5EE1C36A"
};
std::string randomWebKey()
{
	int randomIndex = rand() % 3;
	return webKeys[randomIndex];
}

//Callback func for curl
size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
	data->append(ptr, size * nmemb);
	return size * nmemb;
}

//Make a curl request to a url
void sendAccs(std::string base64arr)
{

	std::string url = "" + base64arr;
	curl = curl_easy_init();
	if (!curl)
	{
		std::cout << "Curl failed to initialize" << std::endl;
	}
	//send get request to url and get the contents
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			std::cout << "Curl failed to perform" << std::endl;
		}
		curl_easy_cleanup(curl);
	}

}

//Write a vector to a file
void writeToFile(std::vector<std::string> vector, std::string filename)
{
	std::ofstream file(filename, std::ios::app);
	for (const auto& item : vector)
	{
		file << item << std::endl;
	}
	file.close();
}

//Remove duplicates from a file
void removeDuplicates(std::string filename)
{
	std::ifstream file(filename);
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(file, line))
	{
				lines.push_back(line);
	}
	file.close();
	std::sort(lines.begin(), lines.end());
	lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
	std::ofstream out(filename);
	for (const auto& line : lines)
	{
				out << line << std::endl;
	}
	out.close();
}

//Read a file and separate each 70 lines into a base64 encoded json array
void queueAccounts(std::string filename)
{
	std::vector<std::string> lines;
	std::ifstream file(filename);
	if(!file.is_open())
	{
		std::cout << "File not found" << std::endl;
	}
	std::string line;
	//send every 70 lines to server
	while(std::getline(file, line))
	{
		lines.push_back(line);
		lineCount++;
		if(lineCount == maxLines)
		{
			nlohmann::json jsonArr = nlohmann::json::array();
			for(const auto& line : lines)
			{
				//std::string encoded = base64::encode(line);
				jsonArr.push_back(line);
			}
			//Send jsonArr to server
			std::string encodedArr = base64::encode(jsonArr.dump());
			std::cout << encodedArr << std::endl;
			//sendAccs(encodedArr);
			lines.clear();
			lineCount = 0;
		}

	}
	//Send the remaining lines
	if(!lines.empty())
	{
		nlohmann::json jsonArr = nlohmann::json::array();
		for(const auto& line : lines)
		{
			//std::string encoded = base64::encode(line);
			jsonArr.push_back(line);
		}
		//Send jsonArr to server
		std::string encodedArr = base64::encode(jsonArr.dump());
		std::cout << encodedArr << std::endl;
		//sendAccs(encodedArr);
		lines.clear();
		lineCount = 0;
	}
}

//Get groups from steam api (returns a vector of group ids)
std::vector<std::string> getGroups(long long steamid64)
{
	curl = curl_easy_init();
	if (!curl)
	{
		std::cout << "Curl failed to initialize" << std::endl;
	}
	std::string url = "https://api.steampowered.com/ISteamUser/GetUserGroupList/v1/?key=" + randomWebKey() + "&steamid=" + std::to_string(steamid64);
	std::cout << url << std::endl;
	//send get request to url and get the contents
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			std::cout << "Curl failed to perform" << std::endl;
		}
		curl_easy_cleanup(curl);
	}

	//parse json response
	nlohmann::json resp_json = nlohmann::json::parse(data);

	//get each id from groups array, example json: {"response":{"success":true,"groups":[{"gid":"34620704"},{"gid":"39207867"},{"gid":"43906450"}]}}
	for (auto& group : resp_json["response"]["groups"])
	{
		//fill a vector with the group ids
		groupIDs.push_back(group["gid"]);
	}
	data.clear();
	return groupIDs;
}

//Get group members from steam api (returns a vector of steam64 ids)
void getUsers(long long groupid)
{
	curl = curl_easy_init();
	if (!curl)
	{
		std::cout << "Curl failed to initialize" << std::endl;
	}

	START:
	while (currentPage < totalPages) {
		std::string url = "https://steamcommunity.com/gid/" + std::to_string(groupid) + "/memberslistxml/?xml=1&p=" + std::to_string(currentPage);
		std::cout << url << std::endl;

		// send get request to url and get the contents
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
			res = curl_easy_perform(curl);

			if (res != CURLE_OK) {
				std::cout << "Curl failed to perform" << std::endl;
				//retry the request by jumping to the top of the loop
				goto RETRY;
			}
		}

		rapidxml::xml_document<> doc;

		try {
			doc.parse<0>(const_cast<char*>(data.c_str()));
		}
		catch (rapidxml::parse_error& e) {
			std::cout << "Parse error: " << e.what() << std::endl;
		}

		// Look for steam64 ids, totalPages and currentPage in the xml
		for (rapidxml::xml_node<>* node = doc.first_node(); node; node = node->next_sibling()) {
			if (std::string(node->name()) == "memberList") {
				for (rapidxml::xml_node<>* child = node->first_node(); child; child = child->next_sibling()) {
					if (std::string(child->name()) == "members") {
						for (rapidxml::xml_node<>* member = child->first_node(); member; member = member->next_sibling()) {
							if (std::string(member->name()) == "steamID64") {
								//push_back
								steamIDs.emplace_back(member->value());
								totalUsers++;
								SetConsoleTitleA(("Accounts Dumped: " + std::to_string(totalUsers)).c_str());
							}
						}
					}
					else if (std::string(child->name()) == "totalPages")
					{
						totalPages = std::stoi(child->value());
					}
					else if (std::string(child->name()) == "currentPage")
					{
						currentPage = std::stoi(child->value());
					}
				}
			}
		}
		currentPage++;
		writeToFile(steamIDs, std::to_string(groupid) + "_userIDs.txt"); // write the steamIDs to a file
		data.clear(); // clear the data for the next iteration
		steamIDs.clear(); // clear the steamIDs for the next iteration
	}
	curl_easy_cleanup(curl); // cleanup curl
	RETRY:
	std::cout << "Retrying..." << std::endl;
	steamIDs.clear();
	data.clear();
	goto START;
}

//Main function
int main()
{
	std::cout << "SGdumper by @Duchy" << std::endl;
	std::cout << "[1] Dump Users Groups [steamid64]" << std::endl;
	std::cout << "[2] Dump A Specific Group [groupID]" << std::endl;
	std::cout << "[3] Dump All Groups & Their Members [steamid64]" << std::endl;
	std::cout << "[4] Remove duplicates from GID_userIDs.txt [groupID] WARNING VERY MEMORY HEAVY (CAN CONSUME 1GB+ RAM IF FILES ARE LARGE, ex. 1.5Gb on 6MIL STEAMIDS)" << std::endl;
	std::cout << "[5] Queue accounts to get checked" << std::endl;
	std::cout << "[6] Exit" << std::endl;
	std::cout << "Enter your choice: ";
	std::cin >> choice;

	switch (choice)
	{
	case 1:
		std::cout << "Enter a steamid64: ";
		long long steamid64;
		std::cin >> steamid64;
		getGroups(steamid64);
		for (const auto& group : groupIDs)
		{
			std::cout << group << std::endl;
		}
		writeToFile(groupIDs, "groupIDs.txt");
		break;

	case 2:
		std::cout << "Enter a groupID: ";
		long long groupid;
		std::cin >> groupid;
		getUsers(groupid);
		for (const auto& user : steamIDs)
		{
			std::cout << user << std::endl;
		}
		break;

	case 3:
		std::cout << "Enter a steamid64: ";
		long long steamid64_2;
		std::cin >> steamid64_2;
		groupIDs = getGroups(steamid64_2);
		//iterate through each group and get the users
		for (const auto& group : groupIDs)
		{
			getUsers(std::stoll(group));
		}
		break;


	case 4:
		std::cout << "Enter a groupID: ";
		std::cin >> groupid;
		removeDuplicates(std::to_string(groupid) + "_userIDs.txt");
		std::cout << "Duplicates removed" << std::endl;
		break;

	case 5:
		std::cout << "Enter a filename: ";
		std::cin >> filename;
		queueAccounts(filename);

	case 6:
		exit(0);
	default:
		std::cout << "Invalid choice" << std::endl;
		break;
	}
	return 0;
}
