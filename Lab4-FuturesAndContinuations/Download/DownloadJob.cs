using System.Diagnostics;
using System.Net.Sockets;
using Lab4_FuturesAndContinuations.Http;

namespace Lab4_FuturesAndContinuations.Download;

/// <summary>State of one download: the socket, the receive buffer, the parser and the outcome.</summary>
public sealed class DownloadJob
{
    private const int BufferSize = 4096;

    public DownloadJob(int id, Uri uri)
    {
        Id = id;
        Uri = uri;
        Request = HttpRequest.BuildGet(uri);
    }

    public int Id { get; }
    public Uri Uri { get; }
    public byte[] Request { get; }
    public Socket Socket { get; } = new(SocketType.Stream, ProtocolType.Tcp);
    public byte[] Buffer { get; } = new byte[BufferSize];
    public HttpResponseParser Parser { get; } = new();
    public Stopwatch Timer { get; } = Stopwatch.StartNew();
    public int ReceiveCalls { get; set; }
    public Exception? Error { get; set; }

    public void Log(string message) => Logger.Log(Id, message);

    public void CloseSocket()
    {
        try
        {
            Socket.Close();
        }
        catch
        {
            // ignored: tearing down anyway
        }
    }
}
