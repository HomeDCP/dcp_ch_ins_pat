// DKDM/EncryptedKDM::as_xml() 크래시 재현기 (GUI 불필요)
// 크래시 스택: EncryptedKDM::as_xml -> ~Document -> xmlpp::Node::free_wrappers  (EXC_BAD_ACCESS)
#include <dcp/encrypted_kdm.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
	if (argc < 2) { std::cerr << "usage: kdm_repro <kdm.xml>\n"; return 2; }
	std::ifstream f(argv[1]);
	std::stringstream ss; ss << f.rdbuf();
	std::string const kdm_xml = ss.str();
	std::cerr << "loaded " << kdm_xml.size() << " bytes\n";

	dcp::EncryptedKDM kdm(kdm_xml);          // 파싱 (libxml++ 로 노드 생성)
	std::cerr << "constructed EncryptedKDM\n";

	for (int i = 0; i < 5; ++i) {
		std::string const out = kdm.as_xml(); // 새 Document 생성->xmlAddID->파괴  <-- 여기서 크래시
		std::cerr << "as_xml() iter " << i << " ok, len=" << out.size() << "\n";
	}
	std::cerr << "DONE (no crash)\n";
	return 0;
}
