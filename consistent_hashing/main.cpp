#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <mutex>
using namespace std;

// 일관적 해싱 구현
class ConsistentHashing {
private:
    map<size_t, string> hashRing;  // 해시 링을 저장하는 맵    hashRing map으로 구현. string을 구조체 변환 필요성
    hash<string> hashFunction;     // 문자열 해시 함수를 사용
    mutex mtx;                     //
public:
    // 생성자
    ConsistentHashing(){};

    /**
     * // 노드를 해시 링에 추가하는 함수 (서버(node)를 동적으로 추가하는 상황을 의미함)
     * @param node
     */
    void addNode(const string& node) {                      // string을 구조체 변환 필요성
        lock_guard<mutex> lock(mtx);                     // mtx.lock() 지양
        size_t hash = hashFunction(node);               // size_t type.
        hashRing[hash] = node;                              // EX : 141231231232 -> Node A
    }


    /**
     * 노드를 해시 링에서 제거하는 함수 (장애 상황 발생을 의미함)
     * @param node
     */
    void removeNode(const string& node) {                   // string을 구조체 변환 필요성
        lock_guard<mutex> lock(mtx);                     // mtx.lock() 지양
        size_t hash = hashFunction(node);               // size_t type
        hashRing.erase(hash);                            // 데이터 재배치 필요하게 되는 상황 야기.
    }

    /**
     * 키를 해시 링에서 적절한 노드로 매핑하는 함수    (node: 반시계방향 cover)
     * @param key
     * @return
     */
    string getNode(const string& key) const {               // string을 구조체 변환 필요성? 여기선 단순 string okay.
        if (hashRing.empty()) {
            return "ERROR";  // not empty() == no server 상태 의미. 해당 위치로 분기되면 안됨.
        }

        size_t hash = hashFunction(key);    // hash(ip) -> map to 인접노드 (시계방향)
        auto it = hashRing.lower_bound(hash);   // lower_bound로 가장 가까운 노드 찾기.

        if (it == hashRing.end()) {
            it = hashRing.begin();  // Circular 모양. 다시 끝이면 12시 방향 가장 우측 Server
        }
        return it->second;  // string 타입
    }


    /**
     * 현재 키의 매핑 상태를 저장하는 함수
     * @param keys
     * @return
     */
    map<string, string> getKeyMappings(const vector<string>& keys) const {
        map<string, string> mappings;
        for (const auto& key : keys) {
            mappings[key] = getNode(key);
        }
        return mappings;
    }

    /**
     * PrintDisplay 전달 목적의 함수
     * @return 해시 링
     */
    map<size_t, string> getHashRing(){
        return hashRing;
    }
};

/**
 * 미 완성된 옵저버 클래스. 옵저버 클래스로 현재 해시 링의 성능을 관찰하고 싶음.
 */
class Observer{
private:
    int threshold;
    map<string, int> checkNodeCount; // 한 노드가 얼마나 많은 키를 커버하는가?
public:
    Observer(int n = 3){
        this->threshold = n;
    }

    int observeState(const map<string, string> &currentState){ // key, node
        for(const auto& [key, node] : currentState){
            checkNodeCount[node]++;
        }

        checkNodeCount.clear();
        return 1;
    }
};

class PrintDisplay{
public:
    // 해시 링의 상태를 출력하는 함수
    void printHashRing(const map<size_t, string>& hashRing){
        for (const auto& [hash, node] : hashRing) {
            cout << hash << " : " << node << "\n";             // string을 구조체 변환 필요성
        }
    }

    // 키 매핑의 변경된 부분을 출력하는 함수
    void printChangedMappings(const map<string, string>& oldMappings, const map<string, string>& newMappings){
        int changes = 0;
        for (const auto& [key, oldNode] : oldMappings) {
            const auto& newNode = newMappings.at(key);
            if (oldNode != newNode) {
                cout << key << " moved from " << oldNode << " to " << newNode << "\n";
                changes++;
            }
            else continue;
        }
        cout << "Total changes: " << changes << "\n";
    }
};

void simulation() {
    ConsistentHashing consistentHashing;
    PrintDisplay printDisplay;
    vector<string> ipAddresses; // 테스트 IP 주소 리스트 만들기

    int userCount = 7; // 접속하는 User IP. 변경 가능.
    // int userCount = 10000;
    for(int i = 0; i < userCount; i++){
        string userIp;
        int a = rand() % 256, b = rand() % 256, c = rand() % 256, d = rand() % 256; // a.b.c.d 의 ip 주소 만들기
        userIp += (to_string(a) + "." + to_string(b) + "." + to_string(c) + "." + to_string(d));
        ipAddresses.push_back(userIp);
    }

    // 난수로 초기 노드 수 결정 (2~6 사이)
    int initialNodeCount = rand() % 4 + 3;          // 다양한 값을 대입 후 시뮬레이션 진행.
    int currentMaxValue = initialNodeCount;

    /**
     * INITAILIZING
     */
    for (int i = 1; i <= initialNodeCount; ++i) {
        consistentHashing.addNode("Node" + to_string(i));
    }

    // 현재 해시 링 상태 출력
    cout << "\nInitial hash ring:\n";
    printDisplay.printHashRing(consistentHashing.getHashRing());

    // 키 매핑 상태 저장
    auto initialMappings = consistentHashing.getKeyMappings(ipAddresses); // for ip. 노드 찾기

    // 초기 키 매핑 결과 출력
    cout << "\nInitial key mappings:\n";
    for (const auto& [key, node] : initialMappings) {
        cout << key << " is mapped to " << node << "\n";
    }

    /**
     * REMOVING SIMULATION
     * 노드 제거 -> 변화된 링 -> 키 매핑 변환 -> 변화를 파악
     */
    // 난수로 제거할 노드 선택
    int nodeToRemove = rand() % initialNodeCount + 1;       // 1 ~ initialNodeCount 임의 삭제.
    consistentHashing.removeNode("Node" + to_string(nodeToRemove));     // Node x

    // 노드 제거 후 해시 링 상태 출력
    cout << "\nHash ring after removing Node" << nodeToRemove << ":\n";
    printDisplay.printHashRing(consistentHashing.getHashRing());

    // 노드 제거 후 키 매핑 상태 저장 = 현재는 전체 일괄 확인?
    auto afterRemovalMappings = consistentHashing.getKeyMappings(ipAddresses);

    // 노드 제거 후 키의 새로운 매핑 결과 출력 및 변경 사항 출력
    cout << "\nKey mappings after removing Node" << nodeToRemove << ":\n";
    for (const auto& [key, node] : afterRemovalMappings) {
        cout << key << " is mapped to " << node << "\n";
    }

    cout << "\nChanges after removing Node" << nodeToRemove << ":\n";
    printDisplay.printChangedMappings(initialMappings, afterRemovalMappings);


    /**
     * ADDING SIMULATION
     * ++노드 추가 -> 변화된 해시 링 -> 키 매핑 변환 -> 변화를 파악
     */
    // 추가할 노드 결정
    int newNode = ++currentMaxValue;  // 새로운 노드번호. 오름차순
    consistentHashing.addNode("Node" + to_string(newNode));

    // 노드 추가 후 해시 링 상태 출력
    cout << "\nHash ring after adding Node" << newNode << ":\n";
    printDisplay.printHashRing(consistentHashing.getHashRing());

    // 노드 추가 후 키 매핑 상태 저장
    auto afterAdditionMappings = consistentHashing.getKeyMappings(ipAddresses);

    // 노드 추가 후 키의 새로운 매핑 결과 출력 및 변경 사항 출력
    cout << "\nKey mappings after adding Node" << newNode << ":\n";
    for (const auto& [key, node] : afterAdditionMappings) {
        cout << key << " is mapped to " << node << "\n";
    }

    cout << "\nChanges after adding Node" << newNode << ":\n";
    printDisplay.printChangedMappings(afterRemovalMappings,afterAdditionMappings);

    cout << "\nSimulation is Finished.\n";
}

int main() {
    ios_base::sync_with_stdio(false);
    cout.tie(nullptr);
    srand((unsigned int)time(nullptr)); // 난수 시드

    /**
     * 노드 수는 2 ~ 6으로 난수 i를 배정 받음. -> 시뮬레이션 함수에서 % n. n을 더 늘려서 실험이 가능함.
     * 삭제는 1 ~ i 번 노드 중 임의 삭제, 삽입은 i + 1 번으로 확정.        삽입 삭제 후 변화를 관찰.
     */
    simulation();   // 시뮬레이션 진행


    /**
     * WHAT TO HASH?
     * 사용자 ID 게시물 ID IP 주소(로드밸런싱) 기타 데이터 식별자
     * Consistent Hashing : 데이터 재배치를 줄인다. 과연 재배치 줄이는 것은 모든 상황에 적합한가? 분산과 재배치의 trade-off
     */
    return 0;
}