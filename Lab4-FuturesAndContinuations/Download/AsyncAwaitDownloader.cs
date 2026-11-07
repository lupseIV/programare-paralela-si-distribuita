using Lab4_FuturesAndContinuations.Net;

namespace Lab4_FuturesAndContinuations.Download;

/// <summary>
/// Implementation 3: the same Task wrappers as implementation 2 (<see cref="SocketTasks"/>),
/// but composed with async/await. The code reads like a sequential blocking client, while the
/// compiler turns every await into a continuation, so no thread is blocked while waiting.
/// </summary>
public static class AsyncAwaitDownloader
{
    public static void Run(IReadOnlyList<DownloadJob> jobs, string outputDir)
    {
        Task[] tasks = jobs.Select(job => Download(job, outputDir)).ToArray();
        Task.WhenAll(tasks).Wait(); // the only Wait(), in Main, after everything is set up
    }

    private static async Task Download(DownloadJob job, string outputDir)
    {
        try
        {
            job.Log($"connecting to {job.Uri.Host}:{job.Uri.Port}");
            await SocketTasks.Connect(job.Socket, job.Uri.Host, job.Uri.Port);

            job.Log("connected, sending request");
            int sent = 0;
            while (sent < job.Request.Length)
            {
                sent += await SocketTasks.Send(job.Socket, job.Request, sent, job.Request.Length - sent);
            }
            job.Log($"request sent ({sent} bytes), waiting for response");

            while (true)
            {
                int n = await SocketTasks.Receive(job.Socket, job.Buffer, 0, job.Buffer.Length);
                job.ReceiveCalls++;
                if (n == 0)
                {
                    job.Parser.OnEndOfStream();
                    break;
                }

                bool hadHeaders = job.Parser.HeadersDone;
                bool complete = job.Parser.Feed(job.Buffer, n);
                if (!hadHeaders && job.Parser.HeadersDone)
                {
                    job.Log($"headers parsed: \"{job.Parser.StatusLine}\", " +
                            $"Content-Length={(job.Parser.ContentLength?.ToString() ?? "none")}");
                }
                if (complete) break;
            }
        }
        catch (Exception e)
        {
            job.Error = e;
        }
        finally
        {
            Report.Finish(job, outputDir);
        }
    }
}
