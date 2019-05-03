using System;
using Xunit;
using Newtonsoft.Json;
using System.Threading.Tasks;
using System.IO;
using System.Net.Sockets;
using System.Net;
using System.Text;
using System.Collections.Concurrent;
using System.Threading;

namespace electrumz_test
{
    public class RPC
    {
        public string jsonrpc { get; set; } = "2.0";
        public string method { get; set; }
        [JsonProperty(PropertyName = "params")]
        public object[] args { get; set; }
        public int id { get; set; }
    }

    public class RPCResponse
    {
        public string jsonrpc { get; set; }
        public object result { get; set; }
        public object error { get; set; }
        public int id { get; set; }
    }

    public static class RPCExt
    {
        public static async Task Send(this Stream stream, RPC c)
        {
            var json = $"{JsonConvert.SerializeObject(c)}\n";
            var data = Encoding.UTF8.GetBytes(json);
            await stream.WriteAsync(data, 0, data.Length);
        }
    }

    public class UnitTest1
    {
        private int CommandId = 0;
        private static ConcurrentDictionary<int, TaskCompletionSource<RPCResponse>> CommandQueue { get; set; } = new ConcurrentDictionary<int, TaskCompletionSource<RPCResponse>>();
        private static Stream Sock { get; set; } = InitRPCSocket();
        private static CancellationTokenSource Cts { get; set; } = new CancellationTokenSource();

        public async Task<RPCResponse> SendAndWait(RPC c)
        {
            var cts = new CancellationTokenSource();
            cts.CancelAfter(TimeSpan.FromSeconds(30));
            
            var tcs = new TaskCompletionSource<RPCResponse>();
            cts.Token.Register(() =>
            {
                tcs.SetCanceled();
            });
            var nid = Interlocked.Increment(ref CommandId);
            c.id = nid;

            if(CommandQueue.TryAdd(nid, tcs))
            {
                await Sock.Send(c);
                return await tcs.Task;
            }
            else
            {
                Console.WriteLine("Failed to add command, skipping..");
            }
            return default;
        }

        private static Stream InitRPCSocket()
        {
            var s = new Socket(SocketType.Stream, ProtocolType.Tcp);
            s.Connect(new IPEndPoint(IPAddress.Loopback, 5555));
            var stream = new NetworkStream(s);

            Task.Factory.StartNew(async () =>
            {
                using (var sr = new StreamReader(stream, Encoding.UTF8, false, 1024, true))
                {
                    while (!Cts.IsCancellationRequested)
                    {
                        var line = await sr.ReadLineAsync();
                        if (!string.IsNullOrEmpty(line))
                        {
                            var json = JsonConvert.DeserializeObject<RPCResponse>(line);
                            if (CommandQueue.ContainsKey(json.id) && CommandQueue.TryRemove(json.id, out TaskCompletionSource<RPCResponse> tcs))
                            {
                                tcs.SetResult(json);
                            }
                            else
                            {
                                Console.WriteLine($"Unknown reply {line}");
                            }
                        }
                        else
                        {
                            Console.WriteLine("Read thread exited");
                            break;
                        }
                    }
                }
            });

            return stream;
        }

        [Fact]
        public async Task TestMethodsAsync()
        {
            var methods = new string[]
            {
                "blockchain.block.header",
                "blockchain.block.headers",
                "blockchain.estimatefee",
                "blockchain.headers.subscribe",
                "blockchain.relayfee",
                "blockchain.scripthash.get_balance",
                "blockchain.scripthash.get_history",
                "blockchain.scripthash.get_mempool",
                "blockchain.scripthash.history",
                "blockchain.scripthash.listunspent",
                "blockchain.scripthash.subscribe",
                "blockchain.scripthash.utxos",
                "blockchain.transaction.broadcast",
                "blockchain.transaction.get",
                "blockchain.transaction.get_merkle",
                "blockchain.transaction.id_from_pos",
                "mempool.changes", 
                "mempool.get_fee_histogram",
                "server.add_peer",
                "server.banner",
                "server.donation_address",
                "server.features",
                "server.peers.subscribe",
                "server.ping",
                "server.version"
            };

            foreach(var m in methods)
            {
                await SendAndWait(new RPC()
                {
                    method = m
                });
            }
        }
    }
}
