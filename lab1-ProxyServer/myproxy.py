#!/usr/bin/python
import socket,re,thread,sys,os,time
import codecs

FOURZEROFOUR="HTTP/1.1 404 Not Found\nContent-Length: 219\nContent-Type: text/html\n\n<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><img src='http://www.climateaccess.org/sites/default/files/Obi%20wan.jpeg'></img><p>Sorry,You Are Not Allowed.</p></body></html>\0"
FOURZEROZERO="HTTP/1.1 400 Bad Request\nContent-Length: 230\nContent-Type: text/html\n\n<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1><img src='http://www.geeksofdoom.com/GoD/img/2007/05/2007-05-25-choke_lg.jpg'></img><p>This HTTP Request is not Allowed</p></body></html>\0"
weblist = ['www.baidu.com']
iplist = ['172.17.235.141','192.168.1.157']
fishlist = ['jwc.hit.edu.cn']
class CacheDict(dict):
	''' Sized dictionary without timeout. '''
	def __init__(self, size=1000):
		dict.__init__(self)
		self._maxsize = size
		self._stack = []

	def __setitem__(self, name, value):
		if len(self._stack) >= self._maxsize:
			self.__delitem__(self._stack[0])
			del self._stack[0]
		self._stack.append(name)
		return dict.__setitem__(self, name, value)

	def get(self, name, default=None, do_set=False):
		try:
			return self.__getitem__(name)
		except KeyError:
			if default is not None:
				if do_set:
					self.__setitem__(name, default)
				return default
			else:
				raise

#sample usage:
# d = SizedDict()
# for i in xrange(10000): d[i] = 'test'
# print len(d)
file_index = 0
cache = CacheDict()
def cache_find(strs):
	global cache
	global file_index
	for i in range(file_index):
		if strs == cache[i]:
			return i
	return -1

def getfileinfo(filename):
	print 'Filename : %s' % filename
	stats = os.stat(filename)
	size = stats[stat.ST_SIZE]
	print 'File Size is %d bytes' % size
	accessed = stats[stat.ST_ATIME]
	modified = stats[stat.ST_MTIME]
	print 'Last accessed: ' + time.ctime(accessed)
	print 'Last modified: ' + time.ctime(modified)
	return time.ctime(modified)

#
# Thread-run function:
#	For each client connection, the proxy will create a new thread.
#	The thread will send and receive actual server's response and send back to client
# Input:
#	conn -- socket connected to the proxy client
#	addr -- client's IP address
#
def processConn(conn,addr):
	# Get HTTP request from client and initialize variables
	request = conn.recv(8092)
	host = ''
	get=''
	redirect=''

	if not request: 
		conn.close()
		return
	lst = request.split("\r\n")
	lst2 = []

	# Extract Host and GET information from the request
	#send('GET %s HTTP/1.1\r\n'%sys.argv[1])
	#s.send('Host: %s\r\n'%sys.argv[1])
	#s.send('Accept: */*\r\n')
	#s.send('\r\n')
	for d in lst:
		if re.search("^Host",d):
			host = d.split(' ')[1]
			#Access Denied.
			#if host in fishlist:
			#	host = 'www.sina.com'
		elif re.search("^GET",d):
			getlst=d.split(' ')
			get = getlst[1]
			if re.search("http://",get):
				get= '/'.join(get.lstrip("http://").split('/')[1:])
				#print 'get####=%s'%get
			get='/'+get
			getlst[1]=get
			d=' '.join(getlst)
		lst2.append(d)
	request='\r\n'.join(lst2)
	print "HTTP request to ",host
	if not host=='':
		# Connect to the host server
		if host in weblist:
			conn.send(FOURZEROZERO)
			conn.close()
			return;
		strs = host+'.html'
		index = cache_find(strs)
		if index != -1:
			getfileinfo(strs)
		c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			c.connect((host, 80))
		except:
			print "[1] connection to %s failed."%host
			conn.send( "connection to %s failed."%host)
			conn.close()
			return
		#c.settimeout(3)
		
		# Send request to the server
		num_sent = c.send(request)
		cdata = ""
		FIRST=True
		content_size=-1
		CONTENT=False
		HTML=False
		CHUNKED=False
		global file_index
		strst = str(file_index)+'.html'
		file_index+=1
		f = open(strst,'w') 
		# Process response in loop
		while 1:
			try:
				line = c.recv(8092)
				# Send whatever content received from server to the client.
				# The proxy does not need to process the client.
				if line=="":
					return
				f.write(line)
				conn.send(line)
				# First segment should contain HTTP response header
				# Extract Content-Length or Encoding type from the header.
				if FIRST:
					print "HTTP reply from ",host
					strlist=(line.split("\r\n\r\n"))
					header=strlist[0].split("\r\n")
					content=""
					for h in header:
						if h.startswith("Content-Length"):
							content_size=int(h.split(" ")[1])
							content=strlist[1]
							CONTENT=True
						if h.startswith("Content-Type: text/html"):
							HTML=True
						if h.startswith("Transfer-Encoding: chunked"):
							CHUNKED=True
					FIRST=False
				else:
					content=line

				#cdata += str(line)
				# If Content-Length is set in header, will count the received size of content until it reaches the threshold
				if CONTENT:
					content_size-=len(content)
					if content_size<=0:
						break
				# If reach the end of HTML tag, also quit loop
				elif re.search("</HTML>",line,re.IGNORECASE):
					break
				# If received a chunked data and reach its end, quit loop
				elif CHUNKED:
					if re.search("\r\n0\r\n\r\n",line):
						break
		
			# Process socket time-out error.
			except socket.timeout:
				c.close()
				#conn.send(cdata)
				print "TIMEOUT ! ERROR !"
				break

			# Process other errors
			except Exception as e:
				#exc_type, exc_obj, exc_tb = sys.exc_info()
				#fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]
				#print(exc_type, fname, exc_tb.tb_lineno)
				#print e
				break
		# Close socket connection to the server
		c.close()
	
	conn.close()


if __name__=="__main__":
	# Accept port number as inputs.
	if len(sys.argv)>2:
		exit(1)
	HOST = ''                 # Symbolic name meaning the local host
	PORT = 28088
	try:
		if len(sys.argv)==2:
			PORT = int(sys.argv[1])             # Arbitrary non-privileged port
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s.bind((HOST, PORT))
		s.listen(1)
	except Exception,e:
		print "Cannot bind the port on %d"%PORT
		print e
		exit(1)

	while 1:
		conn, addr = s.accept()
		if addr[0] in iplist:
			print addr
			conn.send(FOURZEROFOUR)
			continue
		thread.start_new_thread(processConn,(conn,addr))
		#time.sleep(10)