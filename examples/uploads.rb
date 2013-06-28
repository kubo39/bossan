require_relative '../lib/bossan'
require 'rack'
require 'tempfile'


def view_file req
  tempfile = Tempfile.new('raw-upload.')
  req.env['rack.input'].each do |chunk|
    if chunk.respond_to?(:force_encoding)
      tempfile << chunk.force_encoding('UTF-8')
    else
      tempfile << chunk
    end
  end

  req.env['rack.input'].rewind

  tempfile.flush
  tempfile.rewind

  return Rack::Response.new(tempfile,
                            200,
                            { "Content-Length" => req.env["CONTENT_LENGTH"],
                              "Content-Type" => 'image/jpeg'})
end


def upload_file req
  return Rack::Response.new([<<-EOF
<h1>Upload File</h1>
<form action="" method="post" enctype="multipart/form-data">
  <input type="file" name="uploaded_file"><input type="submit" value="Upload">
</form>
EOF
                            ],
                            200,
                            )
end


app = ->(env) {
  req = Rack::Request.new(env)
  resp = if req.request_method == 'POST'
           view_file req
         else
           upload_file req
         end
  # p resp
  return resp
}


Bossan.set_max_content_length(1024 * 1024 * 1024)
Bossan.listen('localhost', 8000)
Bossan.run(app)
