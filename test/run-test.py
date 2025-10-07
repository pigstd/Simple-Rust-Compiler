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
        os.system(f"{test_prog} < {test_file} > build/lexer/{i}.out")
    print("Lexer tests completed.")

def test_parser():
    # test parser
    print("Running parser tests...")
    test_cnt = 6
    test_prog = "build/parser/test_parser"
    for i in range(1, test_cnt + 1):
        print(f"Running test {i}...")
        test_file = f"parser/{i}.in"
        if not os.path.exists(test_file):
            print(f"Test file {test_file} does not exist.")
            continue
        os.system(f"{test_prog} < {test_file} > build/parser/{i}.out")
    print("Parser tests completed.")

def test_const_eval():
    # test const eval
    print("Running const eval tests...")
    test_prog = "build/const_eval/test_const_eval"
    os.system(f"{test_prog} > build/const_eval/test_const_eval.out")
    print("Const eval tests completed.")

# test_lexer()
test_parser()
# test_const_eval()