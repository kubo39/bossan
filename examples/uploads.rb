require_relative '../lib/bossan'
require 'rack'
require 'tempfile'


def view_file req
  # p req
  # p req.env['rack.input']

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

  # return Rack::File.new(tempfile)
  return Rack::Multipart::UploadedFile.new(tempfile, req.content_type, true)
  # return Rack::Response.new(req.env["rack.input"],
  #                           200,
  #                           {"Content-Type" => req.content_type})
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
                            ).to_a
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
Bossan.run('localhost', 8000)
Bossan.listen(app)
