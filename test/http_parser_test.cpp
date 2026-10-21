#include <iostream>
#include <cassert>
#include "http_codec.hpp" // 假设您的代码保存在这个头文件中
#include "bytes_buffer.hpp"

void run_http_parser_test() {
    std::cout << "[Test] 开始执行 HTTP 解析器流式断包测试..." << std::endl;

    http_request_parser parser;
    parser.reset_state();

    // 1. 定义一个完整的、带有 Body 的标准 POST 请求文本
    std::string full_request = 
        "POST /api/v1/login HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 18\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"                  // <-- 注意这里的 \r\n\r\n
        "{\"username\":\"admin\"}"; // <-- Body 刚好 18 字节

    // 2. 故意制造恶劣的网络传输场景：将请求切成 5 个极其零碎的碎片（Chunks）
    // 尤其注意 chunk2 和 chunk3，我们故意把临界的 "\r\n\r\n" 劈开
    std::vector<std::string> chunks = {
        "POST /api/v1/login HT",                               // chunk0: 请求行断开
        "TP/1.1\r\nHost: localhost:8080\r\nContent-Length: ",  // chunk1: Header 键值对断开
        "18\r\nContent-Type: application/json\r\n\r",         // chunk2: 【关键】停在 \r\n\r\n 的第三个字符 '\r'
        "\n{\"username\":\"",                                  // chunk3: 【关键】以 '\n' 开头，且包含一部分 Body（粘包）
        "admin\"}"                                             // chunk4: 最后的 Body 残留碎片
    };

    // 3. 模拟事件循环，一包一包地将数据喂给解析器
    for (size_t i = 0; i < chunks.size(); ++i) {
        std::cout << "-> 正在喂入碎片 [" << i << "], 长度: " << chunks[i].size() << " 字节" << std::endl;
        
        bytes_const_view view(chunks[i].data(), chunks[i].size());
        parser.push_chunk(view);

        // 在喂完最后一包之前，请求不应该结束
        if (i < chunks.size() - 1) {
            assert(parser.request_finished() == false && "错误：请求提前被误判为结束了！");
        }
    }

    // 4. 验证最终解析状态
    std::cout << "\n[验证阶段] 开始比对解析结果..." << std::endl;
    
    // 验证整个请求是否顺利完成
    assert(parser.request_finished() == true && "验证失败：所有数据都喂完了，但解析器认为没收全！");
    std::cout << "✓ 状态验证成功：请求已完全接收。" << std::endl;

    // 验证 Method 和 URL
   // assert(parser.method() == "POST");
    assert(parser.url() == "/api/v1/login");
   // std::cout << "✓ 动作验证成功：Method=[" << parser.method() << "], URL=[" << parser.url() << "]" << std::endl;

    // 验证 Headers 映射表（注意：Key 应该被自动转为了全小写）
    auto& hdrs = parser.headers();
    assert(hdrs.count("host") == 1);
    assert(hdrs["host"] == "localhost:8080");
    
    assert(hdrs.count("content-length") == 1);
    assert(hdrs["content-length"] == "18");
    
    assert(hdrs.count("content-type") == 1);
    assert(hdrs["content-type"] == "application/json");
    std::cout << "✓ 请求头验证成功：所有 Key 已正确转为小写并成功提取。" << std::endl;

    // 验证 Body 内容及其长度
    assert(parser.body().size() == 20);
    assert(parser.body() == "{\"username\":\"admin\"}");
    std::cout << "✓ 请求体验证成功：Body 完整且无数据丢失。内容为: " << parser.body() << std::endl;

    std::cout << "\n🎉 [Congratulation] HTTP 解析器所有流式边界测试通过！" << std::endl;
}

int main() {
    run_http_parser_test();
    return 0;
}
