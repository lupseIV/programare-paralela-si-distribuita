namespace Lab4_FuturesAndContinuations.Download;

/// <summary>
/// Implementation 1: event-driven. The parser is driven directly from the Begin/End callbacks.
/// Each callback finishes its operation and starts the next one:
///   BeginConnect -> OnConnected -> BeginSend -> OnSent -> BeginReceive -> OnReceived -> BeginReceive ...
/// No Tasks are involved; Main waits on a CountdownEvent that every download signals once.
/// </summary>
public static class CallbackDownloader
{
    public static void Run(IReadOnlyList<DownloadJob> jobs, string outputDir)
    {
        using var allDone = new CountdownEvent(jobs.Count);

        foreach (var job in jobs)
        {
            new Session(job, outputDir, () => allDone.Signal()).Start();
        }

        allDone.Wait(); // the only blocking wait, in Main's thread, after everything is set up
    }

    private sealed class Session
    {
        private readonly DownloadJob _job;
        private readonly string _outputDir;
        private readonly Action _onFinished;
        private int _bytesSent;
        private int _finished; // 0/1, guards against finishing twice

        public Session(DownloadJob job, string outputDir, Action onFinished)
        {
            _job = job;
            _outputDir = outputDir;
            _onFinished = onFinished;
        }

        public void Start()
        {
            _job.Log($"connecting to {_job.Uri.Host}:{_job.Uri.Port}");
            try
            {
                _job.Socket.BeginConnect(_job.Uri.Host, _job.Uri.Port, OnConnected, null);
            }
            catch (Exception e)
            {
                Fail(e);
            }
        }

        private void OnConnected(IAsyncResult ar)
        {
            try
            {
                _job.Socket.EndConnect(ar);
                _job.Log("connected, sending request");
                SendRemaining();
            }
            catch (Exception e)
            {
                Fail(e);
            }
        }

        private void SendRemaining()
        {
            _job.Socket.BeginSend(_job.Request, _bytesSent, _job.Request.Length - _bytesSent,
                System.Net.Sockets.SocketFlags.None, OnSent, null);
        }

        private void OnSent(IAsyncResult ar)
        {
            try
            {
                _bytesSent += _job.Socket.EndSend(ar);
                if (_bytesSent < _job.Request.Length)
                {
                    SendRemaining(); // partial send: send the rest
                    return;
                }
                _job.Log($"request sent ({_bytesSent} bytes), waiting for response");
                ReceiveNext();
            }
            catch (Exception e)
            {
                Fail(e);
            }
        }

        private void ReceiveNext()
        {
            _job.Socket.BeginReceive(_job.Buffer, 0, _job.Buffer.Length,
                System.Net.Sockets.SocketFlags.None, OnReceived, null);
        }

        private void OnReceived(IAsyncResult ar)
        {
            try
            {
                int n = _job.Socket.EndReceive(ar);
                _job.ReceiveCalls++;
                if (n == 0)
                {
                    _job.Parser.OnEndOfStream(); // throws if the response is truncated
                    Succeed();
                    return;
                }

                bool hadHeaders = _job.Parser.HeadersDone;
                bool complete = _job.Parser.Feed(_job.Buffer, n);
                if (!hadHeaders && _job.Parser.HeadersDone)
                {
                    _job.Log($"headers parsed: \"{_job.Parser.StatusLine}\", " +
                             $"Content-Length={(_job.Parser.ContentLength?.ToString() ?? "none")}");
                }

                if (complete)
                {
                    Succeed();
                }
                else
                {
                    ReceiveNext();
                }
            }
            catch (Exception e)
            {
                Fail(e);
            }
        }

        private void Succeed() => End(null);

        private void Fail(Exception e) => End(e);

        private void End(Exception? error)
        {
            if (Interlocked.Exchange(ref _finished, 1) == 1) return;
            _job.Error = error;
            try
            {
                Report.Finish(_job, _outputDir);
            }
            finally
            {
                _onFinished();
            }
        }
    }
}
