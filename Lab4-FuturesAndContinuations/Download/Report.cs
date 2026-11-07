namespace Lab4_FuturesAndContinuations.Download;

public static class Report
{
    private static readonly object Lock = new();

    /// <summary>Called exactly once per job when it ends (success or failure). Saves the body and logs the result.</summary>
    public static void Finish(DownloadJob job, string outputDir)
    {
        job.CloseSocket();
        job.Timer.Stop();

        if (job.Error != null)
        {
            job.Log($"FAILED after {job.Timer.ElapsedMilliseconds} ms: {job.Error.GetType().Name}: {job.Error.Message}");
            return;
        }

        var p = job.Parser;
        string path = Path.Combine(outputDir, FileNameFor(job));
        File.WriteAllBytes(path, p.Body);
        job.Log($"DONE in {job.Timer.ElapsedMilliseconds} ms: \"{p.StatusLine}\", " +
                $"Content-Length={(p.ContentLength?.ToString() ?? "none")}, body={p.BodyLength} bytes, " +
                $"{job.ReceiveCalls} receives -> {path}");
    }

    public static void PrintSummary(IEnumerable<DownloadJob> jobs)
    {
        lock (Lock)
        {
            Console.WriteLine();
            Console.WriteLine("==================== Summary ====================");
            foreach (var job in jobs.OrderBy(j => j.Id))
            {
                string result = job.Error != null
                    ? $"ERROR {job.Error.Message}"
                    : $"{job.Parser.StatusCode} {job.Parser.BodyLength,8} bytes";
                Console.WriteLine($"#{job.Id} {result,-30} {job.Timer.ElapsedMilliseconds,6} ms  {job.Uri}");
            }
            foreach (var job in jobs.Where(j => j.Error == null).OrderBy(j => j.Id).Take(1))
            {
                Console.WriteLine();
                Console.WriteLine($"Header lines of #{job.Id}:");
                Console.WriteLine("  " + job.Parser.StatusLine);
                foreach (string line in job.Parser.HeaderLines)
                {
                    Console.WriteLine("  " + line);
                }
            }
        }
    }

    private static string FileNameFor(DownloadJob job)
    {
        string last = job.Uri.Segments.Last().Trim('/');
        if (last.Length == 0) last = "index.html";
        string name = $"{job.Id}-{job.Uri.Host}-{last}";
        foreach (char c in Path.GetInvalidFileNameChars())
        {
            name = name.Replace(c, '_');
        }
        return name;
    }
}
