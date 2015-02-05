from blockchainlistener import Listener
import urllib2

l=Listener(block_confirm_lag=5)

address_pro='1F1RSTvuRiwMjjBJr5QXMYkvGQBV4pAJ9y'
cmdname='firstbitscmds'

while(True):
	localendpoint='http://localhost'
	try:
		bh=int(urllib2.urlopen(localendpoint+'/api/block').read())
	except:
		bh=341200

	
	for b in l.stream_blocks(bh):
		height=b['height']
		txs=b['tx']
		print("Getting blockheight %d" % (height))
		cmdbuf=open(cmdname,'w+')
		cmdbuf.write('%d\n' % (height))
		for tx in txs:	
			if('out' in tx):				#doesn't work with pro.
				for otx in tx['out']:
					if('addr' in otx):
						print(otx['addr'])
						cmdbuf.write('%s %d\n' % (otx['addr'],0))
	
		cmdbuf.close()
