#include <cstdlib>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <list>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <vector>

std::mutex downloadedSizeMutex;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t written = fwrite(ptr, size, nmemb, stream);
  return written;
}
size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

int downloadFile(const char *url, const char *output_filename) {
  CURL *curl;
  FILE *fp;
  CURLcode res;

  // Initialize libcurl
  curl = curl_easy_init();
  if (curl) {
    // Open file for writing
    fp = fopen(output_filename, "wb");
    if (fp == NULL) {
      std::cerr << "Error opening file for writing : " << strerror(errno)
                << std::endl;
      return 1;
    }

    // Set URL to download
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set callback function to write data
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    // Set file handle for writing
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    // Perform the request
    res = curl_easy_perform(curl);

    // Check for errors
    if (res != CURLE_OK) {
      std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                << std::endl;
    }

    // Clean up
    curl_easy_cleanup(curl);
    fclose(fp);
  }

  return 0;
}


int microsoftLogin()
{
  CURL* curl;
  CURLcode res;
  std::string url_devicecode = "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
  std::string url_token = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
  std::string header = "Content-Type: application/x-www-form-urlencoded";
  std::string clientID = "06aa2548-c788-4a1c-9031-22bdd88cd865";
  std::string body_devicecode = "client_id="+clientID+"&scope=XboxLive.signin";
  bool accessTokenReceived = false;
  std::string access_token;
  std::string xblToken;
  std::string uhs;
  std::string xstsToken;
  int XErr;
  std::string MCAccessToken;
  std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

  //**********************
  // DEVICE CODE REQUEST *
  //**********************

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  if (curl) {
      // get device code
      curl_easy_setopt(curl, CURLOPT_URL, url_devicecode.c_str());

      // Set the POST data
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_devicecode.c_str());

      // Set the Content-Type header
      struct curl_slist* headers = NULL;
      headers = curl_slist_append(headers, header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      // Set write callback function
      std::string response_string;
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

      // Perform the request
      res = curl_easy_perform(curl);
      if (res != CURLE_OK) {
          std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
          return 1;
      }

      Json::Value root;
      Json::Reader reader;
      bool parsingSuccessful = reader.parse(response_string, root);
      if (parsingSuccessful) {
      std::string device_code = root["device_code"].asString();
      std::string message = root["message"].asString();
      std::cout<<message<<std::endl;

      curl_slist_free_all(headers);
      curl_easy_reset(curl);

      while (!accessTokenReceived) {
          curl_easy_setopt(curl, CURLOPT_URL, url_token.c_str());

          std::string body_token = "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id="+clientID+"&device_code=" + device_code;
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_token.c_str());

          struct curl_slist* headers_token = NULL;
          headers_token = curl_slist_append(headers_token, header.c_str());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_token);

          response_string.clear();

          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

          res = curl_easy_perform(curl);
          if (res != CURLE_OK) {
              std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
              break;
          }

          parsingSuccessful = reader.parse(response_string, root);
          if (parsingSuccessful && root.isMember("access_token")) {
              access_token = root["access_token"].asString();
              accessTokenReceived = true;
          }
          curl_slist_free_all(headers_token);

          // Check if 15 minutes timeout has been reached
          std::chrono::time_point<std::chrono::system_clock> end_time = std::chrono::system_clock::now();
          std::chrono::duration<double> elapsed_seconds = end_time - start_time;
          if (elapsed_seconds.count() >= 900) {
              std::cerr << "Timeout: Access token not received within 15 minutes" << std::endl;
              break;
          }

          // Wait for 1 second before next request
          std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      curl_easy_cleanup(curl);
      curl_easy_reset(curl);

      if (accessTokenReceived) {
          // Access token received
          std::cout << "Access Token: " << access_token << std::endl;
          curl = curl_easy_init();
          if(curl) {

            //***********************
            // XBL+UHS TOKEN REQUEST *
            //***********************

            std::string xblTokenRequest = "{\"Properties\": {\"AuthMethod\": \"RPS\", \"SiteName\": \"user.auth.xboxlive.com\", \"RpsTicket\": \"d="+access_token+"\"}, \"RelyingParty\": \"http://auth.xboxlive.com\", \"TokenType\": \"JWT\"}";
            struct curl_slist *slist1 = NULL;
            slist1 = curl_slist_append(slist1, "Content-Type: application/json");
            slist1 = curl_slist_append(slist1, "Accept: application/json");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist1);

            curl_easy_setopt(curl, CURLOPT_URL, "https://user.auth.xboxlive.com/user/authenticate");

            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xblTokenRequest.c_str());

            response_string.clear();
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

            res = curl_easy_perform(curl);
            if(res !=CURLE_OK) {
              std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
              return 1;
            }

            //thanks chatGPT, i will refactor this mess later
            parsingSuccessful = reader.parse(response_string, root);
            if (parsingSuccessful) {
              xblToken = root["Token"].asString();
              if (root.isMember("DisplayClaims")) {
              Json::Value displayClaims = root["DisplayClaims"];
              if (displayClaims.isMember("xui")) {
                  Json::Value xuiArray = displayClaims["xui"];
                  if (!xuiArray.empty()) {
                      Json::Value firstElement = xuiArray[0];
                      if (firstElement.isMember("uhs")) {
                          uhs = firstElement["uhs"].asString();
                      } else {
                          std::cerr << "Error: 'uhs' field not found in the first xui element" << std::endl;
                      }
                  } else {
                      std::cerr << "Error: 'xui' array is empty" << std::endl;
                  }
              } else {
                  std::cerr << "Error: 'xui' array not found in 'DisplayClaims' object" << std::endl;
              }
          } else {
              std::cerr << "Error: 'DisplayClaims' object not found" << std::endl;
          }

          }
          std::cout << "uhs value: " << uhs << std::endl;
          std::cout << "xbl Token: " << xblToken << std::endl;

          curl = curl_easy_init();
          if(curl) {

            //*********************
            // XSTS TOKEN REQUEST *
            //*********************

            std::string xstsTokenRequest = "{\"Properties\":{ \"SandboxId\": \"RETAIL\", \"UserTokens\": [\""+xblToken+"\"]},\"RelyingParty\": \"rp://api.minecraftservices.com/\", \"TokenType\":\"JWT\"}";
            struct curl_slist *slist1 = NULL;
            slist1 = curl_slist_append(slist1, "Content-Type: application/json");
            slist1 = curl_slist_append(slist1, "Accept: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist1);

            curl_easy_setopt(curl, CURLOPT_URL, "https://xsts.auth.xboxlive.com/xsts/authorize");

            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xstsTokenRequest.c_str());

            response_string.clear();
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

            res = curl_easy_perform(curl);
            if(res !=CURLE_OK) {
              std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
              return 1;
            }
            parsingSuccessful = reader.parse(response_string, root);
            if (parsingSuccessful && root.isMember("Token")) {
              xstsToken = root["Token"].asString();
            }
            else if(parsingSuccessful && root.isMember("XErr"))
            {
              XErr = root["XErr"].asInt();
              if(XErr == 2148916233){
                std::cout<<"This account doesn't have an Xbox account"<<std::endl;
              }else if(XErr == 2148916235){
                std::cout<<"This account is from a country where Xbox Live is not available/banned"<<std::endl;
              } else if (XErr == 2148916236){
                std::cout<<"This account needs adult verification on Xbox page. (South Korea)"<<std::endl;
              }else if (XErr == 2148916237){
                std::cout<<"This account needs adult verification on Xbox page. (South Korea)"<<std::endl;
              }else if (XErr == 2148916238){
                std::cout<<"This account is a child (under 18) and cannot proceed unless the account is added to a Family by an adult"<<std::endl;
              }
            }
            std::cout<<"xsts Token : "<<xstsToken<<std::endl;
            curl = curl_easy_init();
            if(curl) {
              //**************************
              // MINECRAFT TOKEN REQUEST *
              //**************************

              std::string MCaccessTokenRequest = "{\"identityToken\": \"XBL3.0 x="+uhs+";"+xstsToken+"\"}";
              std::cout<<"request payload : "<<MCaccessTokenRequest<<std::endl;
              struct curl_slist *slist1 = NULL;
              slist1 = curl_slist_append(slist1, "Content-Type: application/json");
              slist1 = curl_slist_append(slist1, "Accept: application/json");

              curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist1);

              curl_easy_setopt(curl, CURLOPT_URL, "https://api.minecraftservices.com/authentication/login_with_xbox");

              curl_easy_setopt(curl, CURLOPT_POSTFIELDS, MCaccessTokenRequest.c_str());

              response_string.clear();
              curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
              curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

              res = curl_easy_perform(curl);
              if(res !=CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                return 1;
              }
              std::cout<<"\n\n MCtoken Requst response : "<<response_string<<std::endl;

              parsingSuccessful = reader.parse(response_string, root);
              if (parsingSuccessful) {
                MCAccessToken = root["access_token"].asString();
              }
              std::cout<<"MCAccesToken : "<<MCAccessToken<<std::endl;

            }
          }
        }

      }

      }
      else {
        std::cerr << "Failed to parse JSON response for device code" << std::endl;
        return 1;
      }

  }

  curl_global_cleanup();
  return 0;
}

int startGame(std::string gamePath, std::string version)
{
  // TEMP STUFF WAITING FOR LOGIN TO BE FINISHED
  std::string instancePath = "version/" + version +".json";
  std::string instanceName;
  std::string version_file;
  int Xss = 1; // TODO : ask for Xss
  int Xmx = 2; // TODO : ask for Xmx (max ram)

  std::string native_directory = "bin";
  std::string minecraft_launcher_brand = "poubelle-launcher";
  std::string minecraft_launcher_version = "0.1";
  std::string log4j_configurationFile = "assets/log_configs/client-1.12.xml";
  std::string username = "MC_USERNAME HERE";
  std::string assetDir = "assets";
  std::string uuid =
      "MC_UUID HERE";
  std::string accessToken ="MC_ACCESS_TOKEN HERE"; // for now manual access to token : https://kqzz.github.io/mc-bearer-token/
                                                   // OR start game from official MC launcher then type in a terminal "ps aux | grep java" and copy the string after "--accessToken" from the outputed command (linux only)
  std::string clientId =
      "P8opNy+42lM9fi0Lkw84RTQi8M6fgWfJwwmy1xbpTROxA0VhK/Ar4R+d6vl4lMLt";
  std::string xuid = "2535413463227249";
  std::string userType = "msa";
  std::string versionType = "release";
  std::string concat_command;

  std::cout<<instancePath<<std::endl;
  std::ifstream file(instancePath);
  Json::Value root;
  file >> root;

  const Json::Value &game = root["arguments"]["game"];
  std::string argValue;
  std::string gameArgs;
  for (const auto &arg : game) {
    if (arg.isString()) {
      if (arg.asString() == "${auth_player_name}") {
        argValue = username;
      } else if (arg.asString() == "${version_name}") {
        argValue = version;
      } else if (arg.asString() == "${game_directory}") {
        argValue = gamePath;
      } else if (arg.asString() == "${assets_root}") {
        argValue = assetDir;
      } else if (arg.asString() == "${assets_index_name}") {
        argValue = root["assets"].asString();;
      } else if (arg.asString() == "${auth_uuid}") {
        argValue = uuid;
      } else if (arg.asString() == "${auth_access_token}") {
        argValue = accessToken;
      } else if (arg.asString() == "${clientid}") {
        argValue = clientId;
      } else if (arg.asString() == "${auth_xuid}") {
        argValue = xuid;
      } else if (arg.asString() == "${user_type}") {
        argValue = userType;
      } else if (arg.asString() == "${version_type}") {
        argValue = versionType;
      } else {
        argValue = arg.asString();
      }

      gameArgs += " " + argValue;
    } else {
      std::cerr << "Warning: Non-string argument found." << std::endl;
    }
  }

  std::string path;
  std::string libPath;
  std::vector<std::string> lib;
  const Json::Value& libraries = root["libraries"];
  for (const auto& library : libraries) {
        // Check if the library has "rules"
        if (!library.isMember("rules") || library["rules"].empty()) {
            // If there are no rules, it's considered cross-platform
            const Json::Value& downloads = library["downloads"]["artifact"];
            path = downloads["path"].asString();
            libPath = "libraries/"+path;
            std::cout<<"loading " <<libPath<<std::endl;
            if (std::filesystem::exists(libPath) && std::filesystem::is_regular_file(libPath))
            {
              lib.push_back(libPath);
            }
            else {
              std::cout<<"missing library : "<< libPath <<std::endl;
            }
        } else {
            // Check if any rule applies to "linux"
            bool linuxRuleFound = false;
            for (const auto& rule : library["rules"]) {
                const Json::Value& os = rule["os"];
                if (os.isMember("name") && os["name"].asString() == "linux") {
                    linuxRuleFound = true;
                    break;
                }
            }
            // If a rule for "linux" was found, include the library
            if (linuxRuleFound) {
                const Json::Value& downloads = library["downloads"]["artifact"];
                std::string path = downloads["path"].asString();
                libPath = "libraries/"+path;
                std::cout<<"loading : " <<libPath<<std::endl;
                if (std::filesystem::exists(libPath) && std::filesystem::is_regular_file(libPath))
                {
                  lib.push_back(libPath);
                }
                else {
                  std::cout<<"missing library : "<< libPath <<std::endl;

                }
            }
        }
    }
  std::string concat_libs;
  for (auto i = lib.begin(); i != lib.end(); ++i)
    concat_libs.append(*i + ":");
  concat_libs.append("version/"+version+".jar");

  std::string jvmArgs;
  const Json::Value &jvm = root["arguments"]["jvm"];

  for (const auto &arg : jvm) {
    if (arg.isString()) {
      std::string argValue = arg.asString();
      size_t pos;

      while ((pos = argValue.find("${natives_directory}")) !=
             std::string::npos) {
        argValue.replace(pos, std::string("${natives_directory}").length(),
                         native_directory);
      }

      while ((pos = argValue.find("${launcher_name}")) != std::string::npos) {
        argValue.replace(pos, std::string("${launcher_name}").length(),
                         minecraft_launcher_brand);
      }

      while ((pos = argValue.find("${launcher_version}")) !=
             std::string::npos) {
        argValue.replace(pos, std::string("${launcher_version}").length(),
                         minecraft_launcher_version);
      }

      while ((pos = argValue.find("${classpath}")) != std::string::npos) {
        argValue.replace(pos, std::string("${classpath}").length(), concat_libs);
      }

      jvmArgs += " " + argValue;
    } else {
      std::cerr << "Warning: Non-string argument found." << std::endl;
    }
  }

  concat_command = std::string("/bin/java ") + "-Xss" + std::to_string(Xss) + "M" + jvmArgs + version_file + " -Xmx" +std::to_string(Xmx) + "G" + " -Dlog4j.configurationFile=" + log4j_configurationFile + " net.minecraft.client.main.Main" + gameArgs;
  std::cout << concat_command << std::endl;
  const char *executed_command = concat_command.c_str();
  system(executed_command);
  //std::cout << "Minecraft started" << std::endl;
  return 0;
}

int main() {
  std::string gamePath;
  std::string instancePath;
  std::string version_file;
  std::string assetID;

  //call for MC_login process (not finished yet, https://help.minecraft.net/hc/en-us/articles/16254801392141p)
  //microsoftLogin();

  if (std::filesystem::exists("version_manifest_v2.json") && std::filesystem::is_regular_file("version_manifest_v2.json")) {
  } else {
    downloadFile("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json","version_manifest_v2.json");
  }

  if (std::filesystem::exists("instances_versions.cfg") && std::filesystem::is_regular_file("instances_versions.cfg")) {
  } else {
    std::ofstream {"instances_versions.cfg"};
  }

  std::cout << "[1] Start an instance [TODO]" << std::endl;
  std::cout << "[2] Create a new instance" << std::endl;
  std::cout << ">> ";
  int startOrNew;
  std::cin >> startOrNew;

  if (startOrNew == 1) {
    size_t pos;
    std::string showInstanceName;
    std::string formatedInstancePath;
    std::vector<std::string> instanceNameList;
    std::vector<std::string>::iterator it;
    // search for instance file one to show the available instances
    for (const auto &entry : std::filesystem::directory_iterator("instances")) {
      formatedInstancePath = entry.path();
      for (char &ch : formatedInstancePath) {
        if(ch == '_'){
        ch = ' ';
      }
      pos = formatedInstancePath.find("/");
      showInstanceName = formatedInstancePath.substr (pos+1);
  }
      instanceNameList.push_back(showInstanceName);
    }
      for (it = instanceNameList.begin(); it != instanceNameList.end(); ++it){
        std::cout << *it << std::endl;
      }
    std::cout << "Which instance?\n>> " << std::endl;
    std::string instanceName;
    std::cin.ignore();
    std::getline(std::cin, instanceName);
    for (it = instanceNameList.begin(); it != instanceNameList.end(); ++it){
      if(instanceName != *it)
      {
        std::cout<<"Entered instance not found"<<std::endl;
      }
      else {
        std::cout<<"instance \""<<showInstanceName<<"\" selected"<<std::endl;


        if (std::filesystem::exists("instances_versions.cfg") && std::filesystem::is_regular_file("instances_versions.cfg")) {
          std::ifstream cFile ("instances_versions.cfg");
          if (cFile.is_open())
          {

              std::string line;
              while(getline(cFile, line)){
                  line.erase(std::remove_if(line.begin(), line.end(), isspace),
                                      line.end());
                  if(line[0] == '#' || line.empty())
                      continue;
                  auto delimiterPos = line.find("=");
                  auto name = line.substr(0, delimiterPos);
                  auto value = line.substr(delimiterPos + 1);
                  for (char &ch : name) {
                    if(ch == ' '){
                      ch = '_';
                    }
                  }
                  if (name == name){
                    std::cout<<name<<" "<<value<<std::endl;
                    std::cout << value << '\n';
                    startGame("instances/"+name, value);

                  }
              }

          }
          else {
              std::cerr << "Couldn't open config file for reading.\n";
          }

        }else{
          std::cout<<"instances_version.cgf not found, if no instances was created, this can be ignored\n but if there are instances, we can't determine instance's version\nyou can recreate the file yourself and add\"{instance_name}={instance_version}\" if you know the version"<<std::endl;
        }
      }
    }

  } else if (startOrNew == 2) {
    std::cout << "Creating new instance" << std::endl;
    std::cout << "Instance's name : ";
    std::string instanceName;
    std::cin.ignore();
    std::getline(std::cin, instanceName);
    gamePath = "instances/"+instanceName;
    std::cout << "Instance's version : ";
    std::string instanceVersion;
    std::getline(std::cin, instanceVersion);
    for (char &ch : instanceName) {
      if (ch == ' ') {
        ch = '_';
      }
    }
    if (std::filesystem::exists(gamePath)) {
      std::cout<<"Instance with same name already taken"<<std::endl;
    } else {
      std::filesystem::create_directories(gamePath);
    }
    std::ifstream file("version_manifest_v2.json");
    Json::Value root;
    file >> root;
    std::string url;
    std::string sha1;
    bool found_version = false;
    const Json::Value &versions = root["versions"];
    for (const auto &version : versions) {
      if (version["id"].asString() == instanceVersion) {
        url = version["url"].asString();
        std::cout << "URL: " << url << std::endl;
        found_version = true;
      }
    }
    if (found_version == false){
      std::cout << "Version \"" << instanceVersion << "\" not found." << ::std::endl;
    }
    std::ofstream outfile;

    outfile.open("instances_versions.cfg", std::ios_base::app);
    std::string data = instanceName+"="+instanceVersion+"\n";
    outfile << data;
    instancePath = "version/" +
                   instanceVersion+".json";
    std::filesystem::create_directories("version/");
    downloadFile(url.c_str(), instancePath.c_str());

    version_file = "version/"+instanceVersion+".jar";
    std::ifstream versionFile(instancePath);
    Json::Value versionFileRoot;
    versionFile >> versionFileRoot;
    const Json::Value &downloadsObject = versionFileRoot["downloads"];

    const Json::Value &clientObject = downloadsObject["client"];

    std::string clientUrl = clientObject["url"].asString();

    if (std::filesystem::exists(version_file) && std::filesystem::is_regular_file(version_file))
    {
      std::cout << "Game's jar already exist, not downloading" << std::endl;
    }
    else{
      std::cout << "Downloading game's jar at : " << version_file<< std::endl;
      downloadFile(clientUrl.c_str(), version_file.c_str());

    }

    const Json::Value &assetIndex = versionFileRoot["assetIndex"];
    assetID = assetIndex["id"].asString();
    std::string assetUrl = assetIndex["url"].asString();

    std::filesystem::create_directories("assets");
    std::filesystem::create_directories("assets/indexes");
    std::filesystem::create_directories("assets/log_configs");
    std::filesystem::create_directories("assets/objects");
    std::filesystem::create_directories("assets/skins");

    std::string assetIndexDownloadPath = "assets/indexes/" + assetID + ".json";
    if (std::filesystem::exists(assetIndexDownloadPath) && std::filesystem::is_regular_file(assetIndexDownloadPath))
      {
        std::cout << "asset index exist, not downloading" << std::endl;
      }
      else {
        downloadFile(assetUrl.c_str(), assetIndexDownloadPath.c_str());
      }

    std::string path;
    std::string libPath;
    std::filesystem::create_directories("libraries/");
    std::string LibPathFolder;
    auto startTime = std::chrono::steady_clock::now();
    const Json::Value& libraries = versionFileRoot["libraries"];
    uint64_t totalSize = 0;
    uint64_t downloadedSize = 0;

    // Loop through each library
    for (const auto& library : libraries) {
        // Check if the library has "rules"
        if (!library.isMember("rules") || library["rules"].empty()) {
            // If there are no rules, it's considered cross-platform
            const Json::Value& downloads = library["downloads"]["artifact"];
            totalSize += downloads["size"].asUInt64();
        } else {
            // Check if any rule applies to "linux"
            bool linuxRuleFound = false;
            for (const auto& rule : library["rules"]) {
                const Json::Value& os = rule["os"];
                if (os.isMember("name") && os["name"].asString() == "linux") {
                    linuxRuleFound = true;
                    break;
                }
            }
            // If a rule for "linux" was found, include the library
            if (linuxRuleFound) {
                const Json::Value& downloads = library["downloads"]["artifact"];
                totalSize += downloads["size"].asUInt64();
            }
        }
    }

    const int numThreadsLibs = 20;
    std::vector<std::thread> Libthreads;
    // Loop through each library again to download
    for (const auto& library : libraries) {
        // Check if the library has "rules"
        if (!library.isMember("rules") || library["rules"].empty()) {
            // If there are no rules, it's considered cross-platform
            const Json::Value& downloads = library["downloads"]["artifact"];
            path = downloads["path"].asString();
            std::string url = downloads["url"].asString();
            libPath = "libraries/"+path;
            size_t found = libPath.find_last_of("/");
            if (found != std::string::npos) {
                LibPathFolder = libPath.substr(0, found + 1);
            }
            std::filesystem::create_directories(LibPathFolder);

            if (std::filesystem::exists(libPath) && std::filesystem::is_regular_file(libPath))
            {
              std::cout << "library exist, not downloading" << std::endl;
            }
            else {
              double progress = (double)downloadedSize / totalSize * 100;
              std::cout << "Downloading "<<std::fixed << std::setprecision(2) << progress << "%"<<" : " << url<< std::endl;
              std::cout<<"cp dl"<<std::endl;
              Libthreads.emplace_back([url, libPath, &downloadedSize, LibPathFolder]() {
                    std::cout<<url.c_str()<<":"<<libPath.c_str()<<std::endl;
                    downloadFile(url.c_str(), libPath.c_str());
                    // Update downloaded size
                    std::lock_guard<std::mutex> lock(downloadedSizeMutex);
                    downloadedSize += std::filesystem::file_size(libPath);
                });
            }
            
        } else {
            // Check if any rule applies to "linux"
            bool linuxRuleFound = false;
            for (const auto& rule : library["rules"]) {
                const Json::Value& os = rule["os"];
                if (os.isMember("name") && os["name"].asString() == "linux") {
                    linuxRuleFound = true;
                    break;
                }
            }
            // If a rule for "linux" was found, include the library
            if (linuxRuleFound) {
                const Json::Value& downloads = library["downloads"]["artifact"];
                std::string path = downloads["path"].asString();
                std::string url = downloads["url"].asString();
                libPath = "libraries/"+path;
                size_t found = libPath.find_last_of("/");
                if (found != std::string::npos) {
                    LibPathFolder = libPath.substr(0, found + 1);
                }
                std::filesystem::create_directories(LibPathFolder);
                if (std::filesystem::exists(libPath) && std::filesystem::is_regular_file(libPath))
                {
                  std::cout << "library exist, not downloading" << std::endl;
                }
                else {
                  double progress = (double)downloadedSize / totalSize * 100;
                  std::cout << "Downloading "<<std::fixed << std::setprecision(2) << progress << "%"<<" : " << url<< std::endl;
                  std::cout<<"linux dl"<<std::endl;
                  Libthreads.emplace_back([url, libPath, &downloadedSize, LibPathFolder]() {
                    std::cout<<url.c_str()<<":"<<libPath.c_str()<<std::endl;
                    downloadFile(url.c_str(), libPath.c_str());
                    // Update downloaded size
                    std::lock_guard<std::mutex> lock(downloadedSizeMutex);
                    downloadedSize += std::filesystem::file_size(libPath);//downloads["size"].asUInt64()
                });
                }
            }
        }

        // Limit the number of concurrent threads
        if (Libthreads.size() >= numThreadsLibs) {
            // Wait for threads to finish
            for (auto& thread : Libthreads) {
              if (thread.joinable())
                thread.join();
            }
            Libthreads.clear();
        }
        for (auto& thread : Libthreads) {
          if (thread.joinable())
            thread.join();
        }
        // Calculate download speed
        auto endTime = std::chrono::steady_clock::now();
        double elapsedTime = std::chrono::duration<double>(endTime - startTime).count();
        double downloadSpeed = (double)downloadedSize / elapsedTime;

        std::stringstream ss;
        if (downloadSpeed < 1024) {
            ss << downloadSpeed << " bytes";
        } else if (downloadSpeed < 1024 * 1024) {
            ss << std::fixed << std::setprecision(2) << (double)downloadSpeed / 1024 << " KB";
        } else if (downloadSpeed < 1024 * 1024 * 1024) {
            ss << std::fixed << std::setprecision(2) << (double)downloadSpeed / (1024 * 1024) << " MB";
        } else {
            ss << std::fixed << std::setprecision(2) << (double)downloadSpeed / (1024 * 1024 * 1024) << " GB";
        }

        // Display download speed in appropriate units
        std::cout << "Download speed: " << ss.str() << "/s" << std::endl;
    }

    Json::Value logging = versionFileRoot["logging"];
    Json::Value client = logging["client"];

    std::string log_config_file= "assets/log_configs/"+client["file"]["id"].asString();
    std::cout<<log_config_file<<std::endl;
    std::cout<<client["file"]["url"].asString()<<std::endl;
    downloadFile(client["file"]["url"].asString().c_str(), log_config_file.c_str());

    std::ifstream assetsIndexFile(assetIndexDownloadPath);
    Json::Value assets;
    assetsIndexFile >> assets;
    const Json::Value objects = assets["objects"];
      // Calculate total size of all files
    totalSize = 0;
    for (const auto& object : objects) {
        totalSize += object["size"].asUInt64();
    }

    // Variables to track progress
    downloadedSize = 0;
    startTime = std::chrono::steady_clock::now();
    const int numThreads = 20; // Adjust as needed
    std::vector<std::thread> threads;
    // Loop through each object to download
    for (Json::Value::const_iterator it = objects.begin(); it != objects.end(); ++it) {
       std::string key = it.key().asString();
      const Json::Value& object = *it;
      std::string hash = object["hash"].asString();
      std::string hashAssetFolder = "assets/objects/" + key.substr(0, key.find_last_of('/'));
      std::filesystem::create_directories(hashAssetFolder);
      std::string assetDownloadURL = "https://resources.download.minecraft.net/" + hash.substr(0, 2) + "/" + hash;
      std::string assetDownloadLocation = "assets/objects/" + key;

        // Display progress percentage
        double progress = (double)downloadedSize / totalSize * 100;
        std::cout << "Downloading " << std::fixed << std::setprecision(2) << progress << "%: " << key << std::endl;

        // Create a thread for downloading each file
        threads.emplace_back([assetDownloadURL, assetDownloadLocation, &downloadedSize, &totalSize]() {
            downloadFile(assetDownloadURL.c_str(), assetDownloadLocation.c_str());
            // Update downloaded size
            std::lock_guard<std::mutex> lock(downloadedSizeMutex);
            downloadedSize += std::filesystem::file_size(assetDownloadLocation);
        });

        // Update downloaded size and file count
        downloadedSize += object["size"].asUInt64();

        // Calculate download speed
        auto endTime = std::chrono::steady_clock::now();
        double elapsedTime = std::chrono::duration<double>(endTime - startTime).count();
        double downloadSpeed = (double)downloadedSize / elapsedTime;

        // Display download speed
        static const char* suffixes[] = {"B", "KB", "MB", "GB"};
        int suffixIndex = 0;
        double size = downloadSpeed;
        while (size >= 1024 && suffixIndex < 3) {
            size /= 1024;
            suffixIndex++;
        }
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << size << " " << suffixes[suffixIndex];
        std::cout << "Download speed: " << ss.str() << "/s" << std::endl;

        // Limit the number of concurrent threads
        if (threads.size() >= numThreads) {
            // Wait for threads to finish
            for (auto& thread : threads) {
                thread.join();
            }
            threads.clear();
        }
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Download complete!" << std::endl;

    std::filesystem::create_directories("bin");



  } else {
    std::cout << "Command not found" << std::endl;
  }
  return 0;
}
