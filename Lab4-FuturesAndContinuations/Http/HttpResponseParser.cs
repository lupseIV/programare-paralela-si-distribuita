using System.Text;

namespace Lab4_FuturesAndContinuations.Http;

/// <summary>
/// Minimal incremental HTTP response parser.
/// Bytes are fed in whatever chunks the socket delivers them. The parser
///   1. collects bytes until the empty line (CRLF CRLF) that ends the header section,
///   2. splits the header section into lines and extracts the Content-Length header,
///   3. collects body bytes until Content-Length bytes have arrived.
/// If the server sends no Content-Length, the body ends when the server closes the connection
/// (we always send "Connection: close").
/// </summary>
public sealed class HttpResponseParser
{
    private readonly MemoryStream _headerBytes = new();
    private readonly MemoryStream _body = new();
    private bool _eof;

    public bool HeadersDone { get; private set; }
    public string StatusLine { get; private set; } = "";
    public int StatusCode { get; private set; }
    public List<string> HeaderLines { get; } = new();
    public long? ContentLength { get; private set; }

    public long BodyLength => _body.Length;
    public byte[] Body => _body.ToArray();

    /// <summary>True when the whole response has been received.</summary>
    public bool IsComplete =>
        HeadersDone && (ContentLength.HasValue ? _body.Length >= ContentLength.Value : _eof);

    /// <summary>Feeds <paramref name="count"/> bytes from <paramref name="buffer"/>. Returns true when the response is complete.</summary>
    public bool Feed(byte[] buffer, int count)
    {
        int i = 0;
        if (!HeadersDone)
        {
            while (i < count)
            {
                _headerBytes.WriteByte(buffer[i++]);
                if (EndsWithBlankLine())
                {
                    ParseHeaders();
                    break;
                }
            }
        }

        if (HeadersDone && i < count)
        {
            int bodyBytes = count - i;
            if (ContentLength.HasValue)
            {
                // never store more than Content-Length bytes
                bodyBytes = (int)Math.Min(bodyBytes, ContentLength.Value - _body.Length);
            }
            _body.Write(buffer, i, bodyBytes);
        }

        return IsComplete;
    }

    /// <summary>Called when the server closed the connection (Receive returned 0 bytes).</summary>
    public void OnEndOfStream()
    {
        _eof = true;
        if (!HeadersDone)
        {
            throw new IOException("Connection closed before the HTTP headers were complete");
        }
        if (ContentLength.HasValue && _body.Length < ContentLength.Value)
        {
            throw new IOException($"Connection closed after {_body.Length} of {ContentLength} body bytes");
        }
    }

    private bool EndsWithBlankLine()
    {
        long n = _headerBytes.Length;
        byte[] b = _headerBytes.GetBuffer();
        bool crlfCrlf = n >= 4 && b[n - 4] == '\r' && b[n - 3] == '\n' && b[n - 2] == '\r' && b[n - 1] == '\n';
        bool lfLf = n >= 2 && b[n - 2] == '\n' && b[n - 1] == '\n'; // tolerate bare-LF servers
        return crlfCrlf || lfLf;
    }

    private void ParseHeaders()
    {
        HeadersDone = true;
        string text = Encoding.ASCII.GetString(_headerBytes.GetBuffer(), 0, (int)_headerBytes.Length);
        string[] lines = text.Split('\n')
            .Select(l => l.TrimEnd('\r'))
            .Where(l => l.Length > 0)
            .ToArray();

        StatusLine = lines.Length > 0 ? lines[0] : "";
        // "HTTP/1.1 200 OK" -> 200
        string[] statusParts = StatusLine.Split(' ', 3);
        if (statusParts.Length >= 2 && int.TryParse(statusParts[1], out int code))
        {
            StatusCode = code;
        }

        foreach (string line in lines.Skip(1))
        {
            HeaderLines.Add(line);
            int colon = line.IndexOf(':');
            if (colon <= 0) continue;

            string name = line[..colon].Trim();
            string value = line[(colon + 1)..].Trim();
            if (name.Equals("Content-Length", StringComparison.OrdinalIgnoreCase) &&
                long.TryParse(value, out long length))
            {
                ContentLength = length;
            }
        }
    }
}
