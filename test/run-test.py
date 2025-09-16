import os
import sys

def test_lexer():
    # test lexer
    # 只要输出每个测试点的输出，手动对比即可
    print("Running lexer tests...")
    test_cnt = 5
    test_prog = "build/lexer/test_lexer"
    for i in range(1, test_cnt + 1):
        print(f"Running test {i}...")
        test_file = f"lexer/{i}.in"
        if not os.path.exists(test_file):
            print(f"Test file {test_file} does not exist.")
            continue
        os.system(f"{test_prog} < {test_file}")
    print("Lexer tests completed.")

test_lexer()