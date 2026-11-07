using System.Net.Sockets;

namespace Lab4_FuturesAndContinuations.Net;

/// <summary>
/// Wraps the Begin/End socket operations into Tasks: the callback sets the result of a
/// TaskCompletionSource (the "promise"), and the caller gets back its Task (the "future").
/// Used by both the ContinueWith implementation and the async/await implementation.
/// </summary>
public static class SocketTasks
{
    public static Task Connect(Socket socket, string host, int port)
    {
        var promise = new TaskCompletionSource();
        try
        {
            socket.BeginConnect(host, port, ar =>
            {
                try
                {
                    socket.EndConnect(ar);
                    promise.SetResult();
                }
                catch (Exception e)
                {
                    promise.SetException(e);
                }
            }, null);
        }
        catch (Exception e)
        {
            promise.SetException(e);
        }
        return promise.Task;
    }

    public static Task<int> Send(Socket socket, byte[] buffer, int offset, int count)
    {
        var promise = new TaskCompletionSource<int>();
        try
        {
            socket.BeginSend(buffer, offset, count, SocketFlags.None, ar =>
            {
                try
                {
                    promise.SetResult(socket.EndSend(ar));
                }
                catch (Exception e)
                {
                    promise.SetException(e);
                }
            }, null);
        }
        catch (Exception e)
        {
            promise.SetException(e);
        }
        return promise.Task;
    }

    public static Task<int> Receive(Socket socket, byte[] buffer, int offset, int count)
    {
        var promise = new TaskCompletionSource<int>();
        try
        {
            socket.BeginReceive(buffer, offset, count, SocketFlags.None, ar =>
            {
                try
                {
                    promise.SetResult(socket.EndReceive(ar));
                }
                catch (Exception e)
                {
                    promise.SetException(e);
                }
            }, null);
        }
        catch (Exception e)
        {
            promise.SetException(e);
        }
        return promise.Task;
    }
}
