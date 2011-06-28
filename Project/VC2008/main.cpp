#include <EAIO/Win32/EAFileStreamWin32.h>
#include <iostream>
using namespace std;

int main()
{
	EA::IO::FileStream st("test.txt");
	st.Open();

	{
		char hoge[100];
		st.Read(&hoge, 100);
		cout << hoge << endl;
	}
	st.Close();
	return 0;
}