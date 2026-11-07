using Lab4_FuturesAndContinuations.Net;

namespace Lab4_FuturesAndContinuations.Download;

/// <summary>
/// Implementation 2 (bonus): connect/send/receive are wrapped in Tasks (see <see cref="SocketTasks"/>)
/// and chained with ContinueWith(). Loops (send-until-all-sent, receive-until-complete) are written as
/// a function that returns a Task and, in its continuation, calls itself again; Unwrap() flattens the
/// resulting Task&lt;Task&gt; into a single Task.
/// Inside a continuation, the antecedent task is already finished, so reading .Result never blocks;
/// if the antecedent failed, .Result rethrows and the failure flows down the chain.
/// </summary>
public static class ContinueWithDownloader
{
    public static void Run(IReadOnlyList<DownloadJob> jobs, string outputDir)
    {
        Task[] tasks = jobs.Select(job => Download(job, outputDir)).ToArray();
        Task.WhenAll(tasks).Wait(); // the only Wait(), in Main, after everything is set up
    }

    private static Task Download(DownloadJob job, string outputDir)
    {
        job.Log($"connecting to {job.Uri.Host}:{job.Uri.Port}");

        return SocketTasks.Connect(job.Socket, job.Uri.Host, job.Uri.Port)
            .ContinueWith(connect =>
            {
                ThrowIfFailed(connect);
                job.Log("connected, sending request");
                return SendAll(job, 0);
            })
            .Unwrap()
            .ContinueWith(send =>
            {
                ThrowIfFailed(send);
                job.Log($"request sent ({job.Request.Length} bytes), waiting for response");
                return ReceiveUntilComplete(job);
            })
            .Unwrap()
            .ContinueWith(receive =>
            {
                // final step: runs on success and on failure of any earlier step
                job.Error = receive.Exception?.GetBaseException();
                Report.Finish(job, outputDir);
            });
    }

    /// <summary>Sends job.Request starting at <paramref name="offset"/>, repeating until everything is sent.</summary>
    private static Task SendAll(DownloadJob job, int offset)
    {
        return SocketTasks.Send(job.Socket, job.Request, offset, job.Request.Length - offset)
            .ContinueWith(send =>
            {
                int sentSoFar = offset + send.Result;
                return sentSoFar < job.Request.Length
                    ? SendAll(job, sentSoFar)
                    : Task.CompletedTask;
            })
            .Unwrap();
    }

    /// <summary>Receives a chunk, feeds it to the parser, and repeats until the response is complete.</summary>
    private static Task ReceiveUntilComplete(DownloadJob job)
    {
        return SocketTasks.Receive(job.Socket, job.Buffer, 0, job.Buffer.Length)
            .ContinueWith(receive =>
            {
                int n = receive.Result;
                job.ReceiveCalls++;
                if (n == 0)
                {
                    job.Parser.OnEndOfStream();
                    return Task.CompletedTask;
                }

                bool hadHeaders = job.Parser.HeadersDone;
                bool complete = job.Parser.Feed(job.Buffer, n);
                if (!hadHeaders && job.Parser.HeadersDone)
                {
                    job.Log($"headers parsed: \"{job.Parser.StatusLine}\", " +
                            $"Content-Length={(job.Parser.ContentLength?.ToString() ?? "none")}");
                }

                return complete ? Task.CompletedTask : ReceiveUntilComplete(job);
            })
            .Unwrap();
    }

    /// <summary>A non-generic Task has no .Result to rethrow through, so propagate its failure explicitly.</summary>
    private static void ThrowIfFailed(Task antecedent)
    {
        if (antecedent.IsFaulted) throw antecedent.Exception!.GetBaseException();
        if (antecedent.IsCanceled) throw new TaskCanceledException(antecedent);
    }
}
