#include "defs.h"
#include "res_load.h"

#ifdef WINDOWS
#include <windows.h>
#else
#include <sys/types.h> // open
#include <sys/stat.h> // open
#include <fcntl.h> // open
#include <unistd.h> // read, write, lseek, close
#endif


static void stub_free(dataBuf* buf) {}
static void free_mem(dataBuf* buf) { free(buf->data); }


#ifdef WINDOWS

static void file_get_module_path(stringbuf* buf)
{
	LPTSTR path = (LPTSTR)stringbuf_reserve(buf, MAX_PATH*sizeof(TCHAR));
	DWORD len = GetModuleFileName(NULL, path, MAX_PATH);
	stringbuf_commit(buf, len);
}

static bool res_load_from_resource(stringref name, dataBuf* result)
{
	HMODULE module = GetModuleHandle(NULL);
	HRSRC resinfo = FindResource(module, strr_cstr(name), "file");
	if (resinfo) {
		HGLOBAL res = LoadResource(module, resinfo);
		if (res) {
			result->data = LockResource(res);
			result->size = SizeofResource(module, resinfo);
			result->free = stub_free;
			return true;
		}
	}
	return false;
}

bool file_read_data(stringref path, dataBuf* result)
{
	HANDLE hFile;
	LARGE_INTEGER size;
	DWORD dwBytes;
	byte* data;

	hFile = CreateFile(strr_cstr(path), GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	if (!GetFileSizeEx(hFile, &size) || size.HighPart != 0) {
		CloseHandle(hFile);
		return false;
	}

	data = cpart_alloc(size.LowPart);
	if (!data) {
		CloseHandle(hFile);
		return false;
	}

	if (!ReadFile(hFile, data, size.LowPart, &dwBytes, NULL))
		dwBytes = 0;

	CloseHandle(hFile);

	if (dwBytes != size.LowPart)
	{
		cpart_free(data);
		return false;
	}

	result->data = data;
	result->free = free_mem;
	result->size = size.LowPart;

	return true;
}

#else // WINDOWS

bool file_read_data(stringref path, dataBuf* result)
{
	byte* data;
	long size;
	int fd;

	// open the file
	fd = open(strr_cstr(path), O_RDONLY | O_BINARY);
	if( fd == -1 ) {
		return false;
		/*if (errno == ENOENT) r2_raise_str(E_FILE, filename);
		if (errno == EACCES) r2_raise_str(E_DENIED, filename);
		if (errno == EINVAL) r2_raise_str(E_ARG_VAL, filename);
		r2_raise_str(E_INVALID, filename);*/
	}

	// get the file size
	size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	if (size < 0) {
		close(fd);
		return false;
		// r2_raise_str(E_INVALID, filename); // lseek fail
	}

	data = malloc(size);

	// read the file
	size = read(fd, data, (unsigned int)size);
	if (size < 0) {
		close(fd);
		free(data);
		return false;
	}

	close( fd );

	result->data = data;
	result->free = free_mem;
	result->size = size;

	return true;
}

#endif // WINDOWS


static stringref c_slash = str_lit("/");
static stringref c_backslash = str_lit("\\");
static stringref c_resources = str_lit("resources\\");

static void file_strip_name(stringbuf* buf)
{
	stringref path = str_ref(buf->s);
	size_t len = strr_length(path);
	size_t bslash = str_rfind(path, c_backslash, -1);
	size_t slash = str_rfind(path, c_slash, -1);

	// find the last slash of either kind.
	if (bslash != -1) {
		if (slash != -1) {
			if (bslash > slash) slash = bslash;
		}
		else slash = bslash;
	}

	if (slash == -1)
		return; // no slash in the path.

	// truncate after the slash.
	stringbuf_truncate(buf, slash + 1);
}

static void file_add_slash(stringbuf* buf)
{
	stringref s = stringbuf_ref(buf);
	if (strr_length(s) == 0 || (
		str_rfind(s, c_slash, -1) != strr_length(s) - 1 &&
		str_rfind(s, c_backslash, -1) != strr_length(s) - 1))
	{
		// buffer is empty or does not end with slash.
		stringbuf_append(buf, c_backslash);
	}
}

static string get_resource_path(stringref path)
{
	stringbuf buf = stringbuf_new(260);
	file_get_module_path(&buf);
	file_strip_name(&buf);
	file_add_slash(&buf);
	stringbuf_append(&buf, c_resources);
	stringbuf_append(&buf, path);
	return stringbuf_finish(&buf);
}

static bool res_load_from_res_file(stringref name, dataBuf* result)
{
	string path = get_resource_path(name);
	stringref path_ref = str_ref(path);
	bool ok = file_read_data(path_ref, result);
	str_release(path);
	return ok;
}

dataBuf res_load(stringref name)
{
	dataBuf buf;

	// check for file placed with the application.
	if (res_load_from_res_file(name, &buf))
		return buf;

#ifdef WINDOWS
	// check for an application resource.
	if (res_load_from_resource(name, &buf))
		return buf;
#endif

	// TODO: check any registered resource loaders.
	// ... need a context to register them in.

	// cannot find the resource.
	buf.size = 0; buf.free = stub_free;
	return buf;
}
